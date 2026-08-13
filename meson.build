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
sources = files(
  'src/main.c',
)

if host_machine.system() == 'darwin'
  sources += files('src/sha256_apple.c')
else
  crypto_dep = dependency('libcrypto', required : true)
  deps += [crypto_dep]
  sources += files('src/sha256_posix.c')
endif

executable('cli-toot', sources,
  dependencies : deps,
)
