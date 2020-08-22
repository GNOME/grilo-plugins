source_tests = [
    'test_tracker3',
]

tracker_sandbox = find_program(tracker3_testutils_dep.get_pkgconfig_variable('command'))
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
        args: [
	    '--index-recursive-directories', datadir,
 	    '--store-tmpdir',
	    exe
	])
endforeach
