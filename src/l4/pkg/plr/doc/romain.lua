-- Simple configuration to redundantly start 'hello'
--
-- As you can see, it's actually only launching the original application
-- and prefixing it with "rom/romain".

package.path = "rom/?.lua";

require("L4");

local ldr = L4.default_loader;

ldr:start(
		  { caps = { },
		    log = {"romain", "c"},
		  },
		    "rom/romain rom/hello"
--		    "rom/romain rom/pthread_hello"
--		    "rom/romain rom/pthread_mutex"
		    );
