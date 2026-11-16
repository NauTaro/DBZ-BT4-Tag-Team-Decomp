-- Requests two local battle cameras. The native port draws them without
-- changing the room, controller ownership or the shared simulation.
return {
    name = "Split screen",
    version = "1.0",
    author = "BT4decomp",
    description = "Two local views in online and offline battles. Your opponent keeps their own display settings.",
    settings = {
        { id = "second_view", label = "Second view", type = "choice",
          choices = { "Current target", "Teammate" }, default = 1,
          tip = "Tag Team: follow your target or a living ally. Local two-player battles retain both players' cameras." },
    },
    on_frame = function(game)
        if game.battle then
            bt3.split_screen(true, bt3.setting("second_view") == 2 and "ally" or "target")
        end
    end,
}
