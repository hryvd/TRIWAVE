const { Redis } = require('@upstash/redis');

const redisUrl = process.env.KV_REST_API_URL || process.env.UPSTASH_REDIS_REST_URL;
const redisToken = process.env.KV_REST_API_TOKEN || process.env.UPSTASH_REDIS_REST_TOKEN;

let redis = null;
if (redisUrl && redisToken) {
  try {
    redis = new Redis({
      url: redisUrl,
      token: redisToken,
    });
  } catch (err) {
    console.error('Failed to initialize Redis:', err);
  }
}

const STATIONS = ['school', 'terminal'];

const authorized = (req) => {
  const expectedKey = process.env.API_KEY || 'triwave_secret_key';
  const headerKey = req.headers['x-api-key'];
  const queryKey = req.query?.key || (req.url && new URL(req.url, 'http://localhost').searchParams.get('key'));
  const providedKey = headerKey || queryKey;
  return providedKey === expectedKey;
};

module.exports = { redis, STATIONS, authorized };
