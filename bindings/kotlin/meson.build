# Kotlin bindings



jni_dep = dependency('jni', required: false)

if not jni_dep.found()
  # Fallback: try to find JNI headers via JAVA_HOME
  # Meson doesn't expose env vars directly, use python to get it
  python_prog = find_program('python3', 'python', required: true)
  java_home = run_command(python_prog, '-c', 'import os; print(os.environ.get("JAVA_HOME", ""))', check: false).stdout().strip()
  
  if java_home != ''
    message('Using JNI from JAVA_HOME: ' + java_home)
    jni_inc_dirs = [
      java_home / 'include',
      java_home / 'include' / 'win32', # Windows
      java_home / 'include' / 'linux', # Linux
      java_home / 'include' / 'darwin', # macOS
    ]
    
    # Filter only existing directories to avoid warnings
    fs = import('fs')
    valid_inc_dirs = []
    foreach d : jni_inc_dirs
      if fs.is_dir(d)
        valid_inc_dirs += d
      endif
    endforeach
    
    jni_dep = declare_dependency(include_directories: include_directories(valid_inc_dirs))
  else
    error('JNI dependency not found and JAVA_HOME not set.')
  endif
endif


# shared_library default prefix on Windows is 'lib', but we might want just 'draconis_jni' or match standard JNI naming.
# However, standard shared_library usage is fine.
draconis_jni = shared_library('draconis_jni',
  'src/main/cpp/draconis_jni.cpp',
  dependencies: [jni_dep, draconis_c_dep],
  install: true,
)
