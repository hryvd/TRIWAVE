const { Redis } = require('@upstash/redis');

const redis = new Redis({
  url: process.env.KV_REST_API_URL || process.env.UPSTASH_REDIS_REST_URL,
  token: process.env.KV_REST_API_TOKEN || process.env.UPSTASH_REDIS_REST_TOKEN,
});

const STATIONS = ['school', 'terminal'];
const authorized = (req) => req.headers['x-api-key'] === (process.env.API_KEY || 'triwave_secret_key');

module.exports = { redis, STATIONS, authorized };
