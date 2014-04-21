
-- Default interaction script. You may edit this directly, or
-- create additional files to point to.
--
-- Called every time interactive mode starts again.
-- Keep state with global variables.
--
-- One way of using the 'interact' method is
-- a custom callback that occurs every time Ctrl-C is pressed.
--
-- It is recommended that you keep utility functions in custom_functions.lua.

--
-- Configuration
--

long_print_on() -- Print objects over multiple lines in repl_run()

--
-- Event loggers
--

function on_add(id) 
end

function on_unfollow(id1, id2) 
    print("UNFOLLOW: ",id1,id2)
end

function on_follow(id1, id2) 
end

function on_tweet(id1, id2) 
end

--
-- Interaction (ie, what happens when ctrl-C is pressed)
--

function on_interact() -- Return true to continue, false to exit
    print("Welcome to Simulator Interactive Mode.")
    print("Type exit() or quit() to (gracefully) finish the simulation.")
    return repl_run() 
end
