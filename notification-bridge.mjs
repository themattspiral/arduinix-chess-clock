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

const player = leftPlayersTurn => leftPlayersTurn ? 'Left' : 'Right';
const topic = leftPlayersTurn => leftPlayersTurn ? TOPIC_LEFT : TOPIC_RIGHT;

const notifyNewGame = (leftPlayersTurn, timerOption) => notifyBoth(`New Game Started! Turn Limit: ${timerOption} | Starting Player: ${player(leftPlayersTurn)}`);
const notifyPlayerTurn = leftPlayersTurn => notify(topic(leftPlayersTurn), 'Your Move!');
const notifyTimeout = leftPlayersTurn => notifyBoth(`Game Over! ${player(leftPlayersTurn)} Timed Out`);

const log = (message, ...rest) => console.log(`${DT_FORMATTER.format(new Date())}: ${message}`, ...rest);

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

    if (data.startsWith("CCNTFY,")) {
        const parts = data.split(',');
        if (parts.length === 4) {
            const [msgType, notificationType, leftPlayersTurn, timerLabel] = parts;
            const isLeft = leftPlayersTurn === '1';

            switch (notificationType) {
                case '0':
                    notifyNewGame(isLeft, timerLabel);
                    break;
                case '1':
                    notifyPlayerTurn(isLeft);
                    break;
                case '2':
                    notifyTimeout(isLeft);
                    break;
            }
        }
    }
});
