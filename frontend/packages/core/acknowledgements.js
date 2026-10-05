// Match only uniquely identifiable receptions in the available message list.
// Two-bit sequence numbers wrap quickly; two minutes is a conservative UI
// window, not a protocol guarantee. Aggregated repeats cannot identify one send.
export const ACK_WINDOW_SECONDS = 120;
const sender = m => m.sender || m.message?.mmsi;
const recipient = m => m.anchor || m.message?.dest_mmsi;
const sequence = n => Number.isInteger(n) && n >= 0 && n <= 3;

export function acknowledgedMessages(messages = []) {
    const confirmed = new Set();
    const originals = messages.filter(m => (m.type === 6 || m.type === 12) && sequence(m.message?.seqno));
    for (const ack of messages) {
        const type = ack.type === 13 ? 12 : ack.type === 7 ? 6 : 0;
        if (!type || !sequence(ack.message?.ack_seqno) || !sender(ack) || !recipient(ack)) continue;
        const candidates = originals.filter(m => m.type === type && sender(m) === recipient(ack) &&
            recipient(m) === sender(ack) && m.message.seqno === ack.message.ack_seqno &&
            Number.isFinite(m.timestamp) && Number.isFinite(ack.timestamp) &&
            ack.timestamp >= m.timestamp && ack.timestamp - m.timestamp <= ACK_WINDOW_SECONDS);
        if (candidates.length === 1 && candidates[0].count === 1) confirmed.add(candidates[0]);
    }
    return confirmed;
}
