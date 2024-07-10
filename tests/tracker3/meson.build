source_tests = []

if not tracker3_testutils_dep.found()
    tracker_sandbox = find_program('localsearch')
    subcommand = ['test-sandbox']
else
    tracker_sandbox = find_program(tracker3_testutils_dep.get_pkgconfig_variable('command'))
    subcommand = []
endif

if tracker_sandbox.found()
    source_tests += ['test_tracker3']
endif

datadir = join_paths(meson.current_source_dir(), 'data')

foreach t: source_tests
    exe = executable(t, t + '.c',
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DTRACKER3_PLUGIN_PATH="@0@/src/tracker3/"'.format(meson.build_root()),
            '-DTRACKER3_DATA_PATH="@0@"'.format(datadir),
        ])
    test(t, tracker_sandbox,
        args: subcommand + [
	    '--index-recursive-directories', datadir,
 	    '--store-tmpdir',
	    exe
	])
endforeach
