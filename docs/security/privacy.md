# Privacy notice

BT4decomp stores game settings, saves and diagnostic files locally. Source-built release packages contain blank game data tables; the installer extracts the required game data from the ISO supplied by the player.

Online features use Supabase for authentication, nicknames, room discovery and competitive results. They process the online account identifier, nickname, room membership and match/rating records. Peers exchange the network endpoints needed to connect. When a direct route is unavailable, the configured TURN provider can relay game traffic and receives the network information required to provide that connection. These providers can also retain connection and security logs under their own policies.

The update and optional-package features contact GitHub to check releases and download selected files. Update checking can occur automatically when enabled. These requests expose ordinary connection metadata such as the requesting IP address to the hosting provider. Language packs and optional textures are downloaded as part of the installation choices made by the player. Game saves and ISO files are not uploaded by the signing pipeline.

Voice chat transmits voice to the other participants when that feature is used. Local diagnostic logs can include connection details; review them before sharing them publicly.

Relevant provider policies are [Supabase](https://supabase.com/privacy), [GitHub](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement), [ExpressTURN](https://www.expressturn.com/privacy) and [SignPath](https://about.signpath.io/privacy-policy). Signing requests contain the application build artifacts and their source/build provenance. They do not contain player sessions or the ISO selected during installation.

Project contact: the [repository issue tracker](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/issues).
