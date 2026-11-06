import { createTurnHandler } from './handler.mjs';
Deno.serve(createTurnHandler({ env: (name: string) => Deno.env.get(name) }));
