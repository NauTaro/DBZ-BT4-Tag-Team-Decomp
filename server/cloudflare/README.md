# Servidor Tag Team BT4

Servicio desplegado: https://bt4-tag-team.mellamannau22.workers.dev
No necesita que quede encendida la PC que hizo el despliegue.

El archivo wrangler.json referencia los recursos de esta cuenta. No contiene
contrasenas ni tokens. La autorizacion de Wrangler se guarda fuera del proyecto.

Restaurar el servicio con la cuenta propietaria autenticada:

    npx --yes wrangler@4.46.0 d1 execute bt4-tag-rooms --remote --file schema.sql
    npx --yes wrangler@4.46.0 deploy

Para otra cuenta hay que crear su propia base D1 y cambiar account_id y database_id.
La migracion tag-v1 crea el Durable Object de sesiones; DB es el binding D1.
schema.sql es idempotente. No se deben copiar credenciales dentro del paquete.

kernel.wasm compila las mismas reglas de sala C que el cliente, incluidas las
barreras de carga, entradas, resultados, movimientos de slots y control del anfitrion.
El ZIP conserva las fuentes C que corresponden al modulo. Antes de cambiar el
protocolo hay que recompilar el modulo y probar clientes y servicio juntos.

Los comandos HTTP de conexion se limitan por IP/minuto; las entradas se transportan
por WebSocket sin una escritura D1 por cuadro. Las salas tienen un limite de diez
conexiones y tres horas. El plan Free de Cloudflare conserva sus propias cuotas.

Voz r13: /v3/voice/tag/CODIGO usa una capacidad temporal emitida solo a un
miembro del WebSocket de juego. /v3/voice/duel/HASH admite las dos plazas con
una capacidad aleatoria compartida por el canal de juego. La voz esta abierta
a ambos equipos; la atenuacion se calcula en el cliente desde las posiciones
de la simulacion. Los mensajes de voz no pasan por el kernel de combate.
La migracion voice-v1 agrega VoiceDuelRoom. No usa Realtime SFU.
El despliegue de voz/proximidad verificado tiene version
ad5119e7-955e-4749-9e78-e2a8762cd717.

R15: aviso de resultados = comando 21. El comando 20 sigue reservado para
la capacidad de voz. No se pueden reutilizar ambos codigos.
