src = ['xml.cpp', 'xml2cpp.cpp', 'generate_stubs.cpp', 'generator_utils.cpp']

dbus_dep = dependency('dbus-1', native: true)
dbusxx_dep = dependency('dbus-c++-1', native: true)
ctemplate_dep = dependency('libctemplate_nothreads', native: true)
xml_dep = dependency('expat', native: true)

# We build this one natively as we only use it to generate files needed for
# later stages in compilation.
dbus_xml2cpp = executable('dbusxx-xml2cpp', src,
    dependencies: [dbus_dep, dbusxx_dep, ctemplate_dep, xml_dep],
    cpp_args: ['-DDATADIR="' + get_option('ctemplate_data_dir') + '"'],
    native: true)