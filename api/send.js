// POST /api/send?to=terminal   body (text/plain): REQUEST,001,CONFIRMED
const { redis, STATIONS, authorized } = require('./_db');

async function parseBody(req) {
  if (typeof req.body === 'string') return req.body;
  if (Buffer.isBuffer(req.body)) return req.body.toString('utf8');
  if (req.body && typeof req.body === 'object') {
    if (typeof req.body.message === 'string') return req.body.message;
    if (typeof req.body.msg === 'string') return req.body.msg;
    const keys = Object.keys(req.body);
    if (keys.length > 0 && req.body[keys[0]] === '') return keys[0];
    return JSON.stringify(req.body);
  }
  return new Promise((resolve) => {
    let data = '';
    req.on('data', (chunk) => { data += chunk; });
    req.on('end', () => resolve(data));
    req.on('error', () => resolve(''));
  });
}

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Headers', 'x-api-key, Content-Type');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  if (req.method === 'OPTIONS') return res.status(200).end();

  if (!authorized(req)) {
    return res.status(401).send('unauthorized: invalid API key');
  }

  if (!redis) {
    return res.status(503).send('Redis not configured. Please connect an Upstash Redis database in Vercel Storage.');
  }

  const to = req.query?.to || (req.url && new URL(req.url, 'http://localhost').searchParams.get('to'));
  if (!STATIONS.includes(to)) {
    return res.status(400).send(`bad request: invalid station '${to}'. Valid stations: ${STATIONS.join(', ')}`);
  }

  try {
    const rawBody = await parseBody(req);
    const msg = (rawBody || '').trim();
    if (!msg || msg.length > 150) {
      return res.status(400).send(`bad request: message empty or exceeds 150 characters (got ${msg.length})`);
    }

    await redis.rpush(`q:${to}`, JSON.stringify({ t: Date.now(), m: msg }));
    await redis.lpush('log', `${new Date().toISOString()}  -> ${to}: ${msg}`);
    await redis.ltrim('log', 0, 49);
    res.status(200).send('ok');
  } catch (err) {
    console.error('Error sending message:', err);
    res.status(500).send('internal server error: ' + err.message);
  }
};
