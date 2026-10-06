// POST /api/send?to=terminal   body (text/plain): REQUEST,001,CONFIRMED
const { redis, STATIONS, authorized } = require('./_db');

module.exports = async (req, res) => {
  if (!authorized(req)) return res.status(401).send('unauthorized');
  const to = req.query.to;
  const msg = typeof req.body === 'string' ? req.body.trim() : '';
  if (req.method !== 'POST' || !STATIONS.includes(to) || !msg || msg.length > 150) {
    return res.status(400).send('bad request');
  }
  await redis.rpush(`q:${to}`, JSON.stringify({ t: Date.now(), m: msg }));
  await redis.lpush('log', `${new Date().toISOString()}  -> ${to}: ${msg}`);
  await redis.ltrim('log', 0, 49);
  res.status(200).send('ok');
};
