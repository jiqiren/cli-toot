project('cli-toot', 'c',
  version : '1.0.1',
  default_options : [
    'c_std=c23',
    'warning_level=2',
    'werror=true',
  ],
)

curl_dep = dependency('libcurl', required : true)

cjson_proj = subproject('cjson')
cjson_dep = cjson_proj.get_variable('libcjson_dep')

common_args = []
if host_machine.system() != 'darwin'
  common_args += ['-D_GNU_SOURCE']
endif

conf_data = configuration_data()
conf_data.set('CLI_TOOT_VERSION', meson.project_version())
version_h = configure_file(
  input : 'src/version.h.in',
  output : 'version.h',
  configuration : conf_data,
)

deps = [curl_dep, cjson_dep]
src = files(
  'src/main.c',
  'src/base64.c',
  'src/http.c',
  'src/json_helpers.c',
  'src/config.c',
  'src/browser.c',
  'src/loopback.c',
  'src/oauth.c',
  'src/toot.c',
)

if host_machine.system() == 'darwin'
  src += files('src/sha256_apple.c')
else
  crypto_dep = dependency('libcrypto', required : true)
  deps += [crypto_dep]
  src += files('src/sha256_posix.c')
endif

executable('cli-toot', src + version_h,
  dependencies : deps,
  c_args : common_args,
)

test('crypto', executable('crypto_test', 'tests/crypto_test.c',
  'src/base64.c',
  (host_machine.system() == 'darwin' ? files('src/sha256_apple.c')
                                     : files('src/sha256_posix.c')),
  include_directories : 'src',
  dependencies : (host_machine.system() == 'darwin' ? []
                                                    : dependency('libcrypto')),
  c_args : common_args,
))

test('config', executable('config_test', 'tests/config_test.c',
  'src/config.c',
  include_directories : 'src',
  c_args : common_args,
))

test('loopback', executable('loopback_test', 'tests/loopback_test.c',
  'src/loopback.c',
  include_directories : 'src',
  c_args : common_args,
))
