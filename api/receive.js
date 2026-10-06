// GET /api/receive?me=school  -> 200 + message text, or 204 when nothing is waiting
const { redis, STATIONS, authorized } = require('./_db');

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Headers', 'x-api-key, Content-Type');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  if (req.method === 'OPTIONS') return res.status(200).end();

  if (!authorized(req)) return res.status(401).send('unauthorized: invalid API key');
  if (!redis) return res.status(503).send('Redis not configured. Please connect an Upstash Redis database in Vercel Storage.');

  const me = req.query?.me || (req.url && new URL(req.url, 'http://localhost').searchParams.get('me'));
  if (!STATIONS.includes(me)) return res.status(400).send(`bad request: invalid station '${me}'. Valid stations: ${STATIONS.join(', ')}`);

  try {
    let item;
    while ((item = await redis.lpop(`q:${me}`)) != null) {
      const v = typeof item === 'string' ? JSON.parse(item) : item;
      if (Date.now() - v.t < 180000) return res.status(200).send(v.m);   // ignore messages older than 3 min
    }
    res.status(204).end();
  } catch (err) {
    console.error('Error receiving message:', err);
    res.status(500).send('internal server error: ' + err.message);
  }
};
