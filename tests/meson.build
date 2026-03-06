test_orage_time_utils = executable (
    'test-orage-time-utils',
    'test-orage-time-utils.c',
    include_directories: include_directories('../src'),
    dependencies: [glib, gtk],
    link_with: [orage_lib],
    install: false
)

test('Orage time utils tests', test_orage_time_utils)
