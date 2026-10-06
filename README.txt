TRIWAVE website (Vercel)
1. Put this folder in a GitHub repo and import it in Vercel (or run `vercel` in this folder).
2. In the Vercel project: Storage -> add an Upstash Redis database (Marketplace) and connect it to the project.
3. Settings -> Environment Variables -> add API_KEY = any secret string you choose.
4. Redeploy. Use the same secret as WEB_KEY in both sketches, and your site's address as WEB_HOST.
