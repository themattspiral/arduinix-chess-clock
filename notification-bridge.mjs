import { SerialPort } from 'serialport';
import { autoDetect } from '@serialport/bindings-cpp';
import { ReadlineParser } from '@serialport/parser-readline';

const DT_FORMATTER = Intl.DateTimeFormat(undefined, { dateStyle: 'short', timeStyle: 'long' });

const ARDUINO_SERIAL_PORT = process.env.ARDUINO_SERIAL_PORT;
const ARDUINO_SERIAL_PORT_SPEED = Number(process.env.ARDUINO_SERIAL_PORT_SPEED);
const TOPIC_LEFT = process.env.TOPIC_LEFT;
const TOPIC_RIGHT = process.env.TOPIC_RIGHT;
const DRY_RUN = process.env.DRY_RUN ? process.env.DRY_RUN.toLowerCase() === 'true' : false;
const DEBUG = process.env.DEBUG ? process.env.DEBUG.toLowerCase() === 'true' : false;

const CLOCK_STATE = {
    '0': 'IDLE',
    '1': 'RUNNING',
    '2': 'TIMEOUT',
    '3': 'MENU',
    '4': 'DEMO'
};

const TIMER_OPTION_LOW_TIME_THRESHOLD = {
    '72h': { ms: 14400000, label: '4h' },
    '48h': { ms: 14400000, label: '4h' },
    '24h': { ms: 3600000, label: '1h' },
    '2h': { ms: 300000, label: '5m' },
    '1h': { ms: 300000, label: '5m' },
    '30m': { ms: 300000, label: '5m' },
    '15m': { ms: 120000, label: '2m' },
    '10m': { ms: 60000, label: '1m' }
    // no reminders for games with <10m timers
};

/*
 * Internal Functions
 */
const notify = (topic, message) => {
    if (DRY_RUN) {
        log(`DRY RUN - NOT POSTing to ntfy.sh on [${topic}]:`, message);
    } else {
        fetch(`https://ntfy.sh/${topic}`, { method: 'POST', body: message });
        log(`POSTing to ntfy.sh on [${topic}]:`, message);
    }
};

const notifyBoth = message => {
    notify(TOPIC_LEFT, message);
    notify(TOPIC_RIGHT, message);
};

const player = leftPlayerTurn => leftPlayerTurn ? 'Left' : 'Right';
const topic = leftPlayerTurn => leftPlayerTurn ? TOPIC_LEFT : TOPIC_RIGHT;

const notifyNewGame = (leftPlayerTurn, timerOption) => notifyBoth(`New Game Started! Turn Limit: ${timerOption} | Starting Player: ${player(leftPlayerTurn)}`);
const notifyPlayerTurn = leftPlayerTurn => notify(topic(leftPlayerTurn), 'Your Move!');
const notifyPlayerTimeLow = (leftPlayerTurn, remainingLabel) => notify(topic(leftPlayerTurn), `Low on time: ${remainingLabel} Remaining!`);
const notifyTimeout = leftPlayerTurn => notifyBoth(`Game Over! ${player(leftPlayerTurn)} Timed Out`);

const log = (message, ...rest) => console.log(`${DT_FORMATTER.format(new Date())}: ${message}`, ...rest);

const parseStatusUpdate = message => {
    const parts = message?.split(',');
    if (parts.length === 5) {
        const [state, timerLabel, leftPlayerTurn, elapsedMs, remainingMs] = parts;
        return {
            state: CLOCK_STATE[state],
            timerLabel,
            leftPlayerTurn: leftPlayerTurn === '1',
            elapsedMs: Number(elapsedMs),
            remainingMs: Number(remainingMs)
        };
    }
};

const handleStatusChange = (last, current) => {
    if (!last) {
        log('Captured Current State:', current.state);
    } else if (last.state === 'IDLE' && current.state === 'RUNNING') {
        log(`New ${current.timerLabel} Game:`, player(current.leftPlayerTurn));
        notifyNewGame(current.leftPlayerTurn, current.timerLabel);
    } else if (last.state === 'RUNNING' && current.state === 'TIMEOUT') {
        log('Timeout - Winner:', player(!current.leftPlayerTurn));
        notifyTimeout(current.leftPlayerTurn);
    } else if (last.state === 'RUNNING' && current.state === 'RUNNING') {
        if (last.leftPlayerTurn !== current.leftPlayerTurn) {
            log('Turn Change:', player(current.leftPlayerTurn));
            notifyPlayerTurn(current.leftPlayerTurn);
        } else if (TIMER_OPTION_LOW_TIME_THRESHOLD[current.timerLabel]
            && last.remainingMs >= TIMER_OPTION_LOW_TIME_THRESHOLD[current.timerLabel].ms
            && current.remainingMs < TIMER_OPTION_LOW_TIME_THRESHOLD[current.timerLabel].ms
        ) {
            const label = TIMER_OPTION_LOW_TIME_THRESHOLD[current.timerLabel].label;
            log(`Low Time (${label} remaining):`, player(current.leftPlayerTurn));
            notifyPlayerTimeLow(current.leftPlayerTurn, label);
        }
    } else if (last.state !== current.state) {
        log(`State Change:`, current.state);
    }
};

const listPorts = async () => {
    const Binding = autoDetect();
    const ports = await Binding.list();

    log(`Found ${ports.length} Serial Port${ports.length > 1 ? 's' : ''}:`, ports.map(port => 
        `${port.path} | ${port.friendlyName} (${port.manufacturer}) at ${port.locationId}`
    ));
};

/*
 * Main Run
 */
console.log(`Arduinix Chess Clock ${process.env.npm_package_version} - Notification Bridge
  Serial Port: ${ARDUINO_SERIAL_PORT}
  Serial Port Speed: ${ARDUINO_SERIAL_PORT_SPEED}
  Left Player NTFY Topic: ${TOPIC_LEFT}
  Right Player NTFY Topic: ${TOPIC_RIGHT}
  Dry Run: ${DRY_RUN}
`);

const port = new SerialPort({ path: ARDUINO_SERIAL_PORT, baudRate: ARDUINO_SERIAL_PORT_SPEED });
const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

let lastStatus = null;

port.on('error', async err => {
    log(`Error Opening Arduino Serial Port at ${ARDUINO_SERIAL_PORT} (${ARDUINO_SERIAL_PORT_SPEED} baud):`, err);
    listPorts();
});
port.on('close', err => {
    log(`Arduino Serial Port Closed: ${ARDUINO_SERIAL_PORT}`);
});
port.on('open', () => {
  log(`Arduino Serial Port Opened: ${ARDUINO_SERIAL_PORT} (${ARDUINO_SERIAL_PORT_SPEED} baud)`);
});

parser.on('data', data =>{
    if (DEBUG) {
        log('DEBUG:', data);
    }
    let status = parseStatusUpdate(data);

    if (status) {
        handleStatusChange(lastStatus, status);
    } else {
        log('Error processing status:', data);
    }

    lastStatus = status;
});
