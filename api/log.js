// GET /api/log  -> last 20 events (used by the dashboard)
const { redis, authorized } = require('./_db');

module.exports = async (req, res) => {
  if (!authorized(req)) return res.status(401).send('unauthorized');
  res.status(200).json(await redis.lrange('log', 0, 19));
};
