export const config = {
  gatewayPort: Number(process.env.HELIX_GATEWAY_PORT ?? 3001),
  engineHost: process.env.HELIX_ENGINE_HOST ?? "127.0.0.1",
  enginePort: Number(process.env.HELIX_ENGINE_PORT ?? 9001),
  databaseUrl:
    process.env.DATABASE_URL ?? "postgres://helix:helix@127.0.0.1:5432/helix",
  sessionTtlHours: Number(process.env.SESSION_TTL_HOURS ?? 12),
};
