-- vi: set et
package.path = "rom/?.lua";

require ("Aw");

local ldr = L4.default_loader;
local ankh_vbus;
local shm_morpork, shm_morping, shm_morpong;

function ankh_channels()
    ankh_vbus = ldr:new_channel();
    shm_morpork = ldr:create_namespace({});
    shm_morping = ldr:create_namespace({});
    shm_morpong = ldr:create_namespace({});
end


function ankh_io()
    Aw.io({ankh = ankh_vbus}, "-vv", "rom/ankh.vbus");
end


-- Start ANKH
--
-- Parameters:
--   iobus := an Io virtual PCI bus that contains the NIC devices
--            you want to drive, may be an empty bus if you only
--            want to use loopback
--   nstab := a table containing (name->name) space mappings for
--            the SHMC name spaces this server instance should
--            have access to
--
-- Returns: An Ankh service channel cap that you can then use to
--          start clients.
function ankh_server(iobus, nstab, ...)

    if type(nstab) ~= "table" then
        print("2nd parameter to ankh_server() must be a table!");
        nstab = {};
    end

    local ankh_chan = ldr:new_channel();

    local ankh_caps = {
        rom = rom;                          -- ROM
        ankh_service = ankh_chan:svr();     -- Service cap
        vbus = iobus;                       -- The PCI bus
    };

    -- and add all shm name spaces passed
    for k,v in pairs(nstab) do
        ankh_caps[k] = v;
    end

    ldr:startv({
        caps = ankh_caps,
        log  = {"ankh", "green"},
        l4re_dbg = L4.Dbg.Warn
        },
        "rom/ankh"
    );

    return ankh_chan;
end

-- Start an ANKH client
--
-- Parameters:
--   binary         := binary to start
--   parameters     := table of parameters to pass to the binary
--   ankh_channel   := service channel to open ankh session
--   ankh_config    := session configuration
--   nstab          := table of name->ns mappings this client should
--                     get access to (SHMC)
function ankh_client(binary, parameters, env, ankh_channel, ankh_config, nstab)
    env.caps.ankh = ankh_channel:create(0, ankh_config);

    -- and add all shm name spaces passed
    for k,v in pairs(nstab) do
        env.caps[k] = v;
    end

    ldr:startv(env,
               binary, unpack(parameters));
end

-- ================ GENERIC SETUP ====================
ankh_channels();
ankh_io();
local chan = ankh_server(ankh_vbus, {shm_morpork = shm_morpork:m("rws"),
                                     shm_morping = shm_morping:m("rws"),
                                     shm_morpong = shm_morpong:m("rws")}
                        );


-- ================ CLIENT SETUP ====================
-- MORPORK
--ankh_client("rom/morpork",
--            { "shm_morpork", "16384" },
--            { caps = { rom = rom }, log = {"morpork", "cyan"} },
--            chan,
--            "debug,promisc,device=eth0,shm=shm_morpork,bufsize=16384,phys_mac",
--            { shm_morpork = shm_morpork:m("rws") }
--);


-- PINGPONG
--ankh_client("rom/morping",
--            { "shm_morping" },
--            { caps = { rom = rom }, log = {"morping", "Yellow"} },
--            chan,
--            "debug,promisc,device=lo,shm=shm_morping,bufsize=2048",
--            { shm_morping = shm_morping:m("rws") }
--);
--ankh_client("rom/morpong",
--            { "shm_morpong" },
--            { caps = { rom = rom }, log = {"morpong", "yellow"} },
--            chan,
--            "debug,promisc,device=lo,shm=shm_morpong,bufsize=2048",
--            { shm_morpong = shm_morpong:m("rws") }
--);

-- WGET
ankh_client("rom/wget_clnt",
            { "--shm", "shm_morpork", "--bufsize", "16384", "-v", "http://de.archive.ubuntu.com/ubuntu-releases/lucid/ubuntu-10.04-desktop-i386.iso" },
            { caps = { rom = rom }, log = {"wget", "cyan"} },
            chan,
            "debug,promisc,device=eth0,shm=shm_morpork,bufsize=16384,phys_mac",
            { shm_morpork = shm_morpork:m("rws") }
);
