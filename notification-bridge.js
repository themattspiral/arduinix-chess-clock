const TOPIC_LEFT = 'md1227-chess-clock-left';
const TOPIC_RIGHT = 'md1227-chess-clock-right';

const notify = (topic, message) => fetch(`https://ntfy.sh/${topic}`, { method: 'POST', body: message });
const notifyBoth = message => {
    notify(TOPIC_LEFT, message);
    notify(TOPIC_RIGHT, message);
};

const player = leftPlayerTurn => leftPlayerTurn ? 'Left' : 'Right';
const topic = leftPlayerTurn => leftPlayerTurn ? TOPIC_LEFT : TOPIC_RIGHT;

const notifyNewGame = (leftPlayerTurn, timerOption) => notifyBoth(`New Game Started! Turn Limit: ${timerOption} Starting Player: ${player(leftPlayerTurn)}`);
const notifyPlayerTurn = leftPlayerTurn => notify(topic(leftPlayerTurn), 'Your move!');
const notifyPlayerTimeLow = (leftPlayerTurn, remainingMS) => notify(topic(leftPlayerTurn), `Low on time: ${remainingMS / 1000} sec remaining!`);
const notifyTimeout = leftPlayerTurn => notifyBoth(`Game Over! ${player(leftPlayerTurn)} timed out - ${player(!leftPlayerTurn)} wins!`);


