// A private, local credential for the pilot. The production build omits this loader.
#if defined(BT4_TAG_TURN_PILOT)
struct TagTurnPilot {
    std::string uri;
    bool forceRelay = false;
    std::string server, username, password;

    static TagTurnPilot load(bool log = true) {
        const char *overridePath = SDL_getenv("BT4_TAG_PILOT_TURN_FILE");
        const char *base = SDL_GetBasePath();
        std::string path = overridePath ? overridePath : std::string(base ? base : "") + "bt4-turn-pilot.json";
        SDL_IOStream *file = SDL_IOFromFile(path.c_str(), "rb");
        if (!file) {
            if (log) fprintf(stderr, "tag P2P pilot: no local TURN configuration\n");
            return {};
        }
        char bytes[4097];
        const size_t size = SDL_ReadIO(file, bytes, sizeof(bytes));
        SDL_CloseIO(file);
        auto config = size && size < sizeof(bytes) ? Pj::parse(bytes, bytes + size, nullptr, false) : Pj();
        if (config.is_object()) {
            const auto host = text(config, "hostname");
            const auto user = text(config, "username");
            const auto pass = text(config, "credential");
            const auto port = number(config, "udp_port");
            const auto force = config.find("force_relay");
            if (host.size() >= 2 && host.size() <= 253 &&
                host.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-") == std::string::npos &&
                !user.empty() && user.size() <= 128 && !pass.empty() && pass.size() <= 128 && port && port <= 65535 &&
                (force == config.end() || force->is_boolean())) {
                const bool relayOnly = force != config.end() && force->get<bool>();
                const auto server = host + ":" + std::to_string(port);
                if (log) fprintf(stderr, "tag P2P pilot: TURN configured, relay_only=%d\n", (int)relayOnly);
                return {"turn:" + iceCredential(user) + ":" + iceCredential(pass) + "@" + server +
                        "?transport=udp", relayOnly, server, user, pass};
            }
        }
        if (log) fprintf(stderr, "tag P2P pilot: invalid local TURN configuration\n");
        return {};
    }
};
#endif
