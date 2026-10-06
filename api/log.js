// GET /api/log  -> last 20 events (used by the dashboard)
const { redis, authorized } = require('./_db');

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Headers', 'x-api-key, Content-Type');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  if (req.method === 'OPTIONS') return res.status(200).end();

  if (!authorized(req)) return res.status(401).json({ error: 'unauthorized: invalid API key' });
  if (!redis) return res.status(503).json({ error: 'Redis not configured. Please connect an Upstash Redis database in Vercel Storage.' });

  try {
    const logs = await redis.lrange('log', 0, 19);
    res.status(200).json(logs || []);
  } catch (err) {
    console.error('Error fetching logs:', err);
    res.status(500).json({ error: 'internal server error: ' + err.message });
  }
};
