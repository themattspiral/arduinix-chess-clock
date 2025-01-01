import { SerialPort } from 'serialport';
import { autoDetect } from '@serialport/bindings-cpp';
import { ReadlineParser } from '@serialport/parser-readline';

const ARDUINO_SERIAL_PORT = process.env.ARDUINO_SERIAL_PORT;
const ARDUINO_SERIAL_PORT_SPEED = Number(process.env.ARDUINO_SERIAL_PORT_SPEED);
const TOPIC_LEFT = process.env.TOPIC_LEFT;
const TOPIC_RIGHT = process.env.TOPIC_RIGHT;

const CLOCK_STATE = {
    '0': 'IDLE',
    '1': 'RUNNING',
    '2': 'TIMEOUT',
    '3': 'MENU',
    '4': 'DEMO'
};

const TIMER_OPTION_LOW_TIME_THRESHOLD = {
    '72h': { ms: 3600000, label: '1h' },
    '48h': { ms: 3600000, label: '1h' },
    '24h': { ms: 3600000, label: '1h' },
    '2h': { ms: 300000, label: '5m' },
    '1h': { ms: 300000, label: '5m' },
    '30m': { ms: 300000, label: '5m' },
    '15m': { ms: 60000, label: '1m' },
    '10m': { ms: 60000, label: '1m' }
};

/*
 * Internal Functions
 */
const notify = (topic, message) => fetch(`https://ntfy.sh/${topic}`, { method: 'POST', body: message });
const notifyBoth = message => {
    notify(TOPIC_LEFT, message);
    notify(TOPIC_RIGHT, message);
};

const player = leftPlayerTurn => leftPlayerTurn ? 'Left' : 'Right';
const topic = leftPlayerTurn => leftPlayerTurn ? TOPIC_LEFT : TOPIC_RIGHT;

const notifyNewGame = (leftPlayerTurn, timerOption) => notifyBoth(`New Game Started! Turn Limit: ${timerOption} | Starting Player: ${player(leftPlayerTurn)}`);
const notifyPlayerTurn = leftPlayerTurn => notify(topic(leftPlayerTurn), 'Your Move!');
const notifyPlayerTimeLow = (leftPlayerTurn, remainingLabel) => notify(topic(leftPlayerTurn), `Low on time! ${remainingLabel} Remaining!`);
const notifyTimeout = leftPlayerTurn => notifyBoth(`Game Over! ${player(leftPlayerTurn)} Timed Out - ${player(!leftPlayerTurn)} Wins!`);

const log = (message, ...rest) => console.log(`${new Date()}: ${message}`, ...rest);

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
    log(`Found ${ports.length} Serial Port${ports.length > 1 ? 's' : ''}:`, ports);
};

/*
 * Main Run
 */

// open serial communication with Arduino
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
    let status = parseStatusUpdate(data);

    if (status) {
        handleStatusChange(lastStatus, status);
    } else {
        log('Error processing status:', data);
    }

    lastStatus = status;
});
