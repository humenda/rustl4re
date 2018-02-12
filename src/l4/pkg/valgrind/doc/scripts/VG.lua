module ("VG", package.seeall);

require ("L4");

-- Start Valgrind
-- 
-- Available options:
--      * tool                Valgrind tool (string: none, memcheck)
--      * cmdline             application cmd line to start (table of strings)
--      * debuglevel=<int>    Valgrind "-d" option count
--      * verbosity=<int>     Valgrind "-v" option count
function valgrind(options)
	local idx = 2;
	local args = {};

	local toolbinaries = {
		none = "none-x86-l4re",
		memcheck = "memcheck-x86-l4re"
	};

	args[1] = "rom/"..toolbinaries[options.tool];

	for i = 1, options.debuglevel do
		args[idx] = "-d";
		idx = idx + 1;
	end

	for i = 1, options.verbosity do
		args[idx] = "-v";
		idx = idx + 1;
	end

	for k,v in ipairs(options.cmdline) do
	    args[idx] = v;
		idx = idx + 1;
	end

	L4.default_loader:startv({log = {"vg", r}},
							 unpack(args));
end


-- Build default options for starting Valgrind
function default_args(cmdline, ...)
	args = {}
	args.debuglevel = 0;
	args.verbosity  = 0;
	args.cmdline     = {cmdline, ...};
	args.tool       = "none";

	return args;
end


-- Launch the nulgrind tool
function none(cmdline, ...)
	args = default_args(cmdline, ...)
	args.tool = "none";
	args.debuglevel = 4;
	args.verbosity = 4;
	valgrind(args);
end


-- Launch memcheck
function memcheck(cmdline, ...)
	args = default_args(cmdline, ...)
	args.tool = "memcheck";
	args.cmdline = { "--leak-check=yes", "--show-reachable=yes", cmdline, ... };
	valgrind(args);
end

function debug_memcheck(cmdline, ...)
	args = {}
	args.debuglevel = 4;
	args.verbosity  = 4;
	args.cmdline    = { "--leak-check=yes", "--show-reachable=yes", cmdline, ... };
	args.tool = "memcheck";
	valgrind(args);
end
