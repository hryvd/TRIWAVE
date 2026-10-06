// GET /api/receive?me=school  -> 200 + message text, or 204 when nothing is waiting
const { redis, STATIONS, authorized } = require('./_db');

module.exports = async (req, res) => {
  if (!authorized(req)) return res.status(401).send('unauthorized');
  const me = req.query.me;
  if (!STATIONS.includes(me)) return res.status(400).send('bad request');

  let item;
  while ((item = await redis.lpop(`q:${me}`)) != null) {
    const v = typeof item === 'string' ? JSON.parse(item) : item;
    if (Date.now() - v.t < 180000) return res.status(200).send(v.m);   // ignore messages older than 3 min
  }
  res.status(204).end();
};
