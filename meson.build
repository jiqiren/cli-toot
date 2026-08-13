project('cli-toot', 'c',
  version : '0.1.0',
  default_options : [
    'c_std=c23',
    'warning_level=2',
    'werror=true',
  ],
)

curl_dep = dependency('libcurl', required : true)

cjson_proj = subproject('cjson')
cjson_dep = cjson_proj.get_variable('libcjson_dep')

deps = [curl_dep, cjson_dep]
src = files(
  'src/main.c',
  'src/base64.c',
  'src/http.c',
)

if host_machine.system() == 'darwin'
  src += files('src/sha256_apple.c')
else
  crypto_dep = dependency('libcrypto', required : true)
  deps += [crypto_dep]
  src += files('src/sha256_posix.c')
endif

executable('cli-toot', src,
  dependencies : deps,
)

test('crypto', executable('crypto_test', 'tests/crypto_test.c',
  'src/base64.c',
  (host_machine.system() == 'darwin' ? files('src/sha256_apple.c')
                                     : files('src/sha256_posix.c')),
  include_directories : 'src',
  dependencies : (host_machine.system() == 'darwin' ? []
                                                    : dependency('libcrypto')),
))
