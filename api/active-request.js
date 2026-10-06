// GET /api/active-request -> returns current pickup request status
const { redis, authorized } = require('./_db');
const { DRIVERS } = require('./drivers');

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Headers', 'x-api-key, Content-Type');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  if (req.method === 'OPTIONS') return res.status(200).end();

  if (!authorized(req)) {
    return res.status(401).json({ error: 'unauthorized: invalid API key' });
  }

  if (!redis) {
    return res.status(503).json({ error: 'Redis not configured' });
  }

  try {
    const raw = await redis.get('active_request');
    if (!raw) {
      return res.status(200).json({ active: false, status: 'idle' });
    }

    const data = typeof raw === 'string' ? JSON.parse(raw) : raw;
    const ageMs = Date.now() - (data.timestamp || 0);

    // After 3 minutes (180s), requests expire (same as ESP32 timeout)
    if (ageMs > 180000) {
      return res.status(200).json({
        active: false,
        status: 'expired',
        reqId: data.reqId,
        ageSeconds: Math.floor(ageMs / 1000)
      });
    }

    let driverInfo = null;
    if (data.acceptedBy) {
      driverInfo = DRIVERS.find(d => d.id === data.acceptedBy) || { id: data.acceptedBy, name: 'Driver #' + data.acceptedBy, plate: 'N/A' };
    }

    return res.status(200).json({
      active: true,
      reqId: data.reqId,
      status: data.status, // 'pending' | 'accepted'
      timestamp: data.timestamp,
      ageSeconds: Math.floor(ageMs / 1000),
      expiresInSeconds: Math.max(0, Math.floor((180000 - ageMs) / 1000)),
      acceptedBy: data.acceptedBy || null,
      acceptedDriver: driverInfo,
      raw: data.raw
    });
  } catch (err) {
    console.error('Error in active-request:', err);
    res.status(500).json({ error: 'server error: ' + err.message });
  }
};
