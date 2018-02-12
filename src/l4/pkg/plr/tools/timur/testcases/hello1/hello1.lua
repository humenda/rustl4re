-- Simple configuration to redundantly start 'hello'
--
-- As you can see, it's actually only launching the original application
-- and prefixing it with "rom/romain".

package.path = "rom/?.lua";

require("L4");
require("Aw");

local ldr = L4.default_loader;

ldr:start(
		  { caps = { },
		    log = {"romain", "b"},
		  },
		    "rom/romain rom/hello"
		    );
