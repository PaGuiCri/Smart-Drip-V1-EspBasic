Import("env")
from shutil import copytree, rmtree
from os.path import exists, join

def before_buildfs(source, target, env):
    config_dir = join(env['PROJECT_DIR'], "config")
    data_dir = join(env['PROJECT_DIR'], "data")

    # Copiamos el contenido de 'config/' a 'data/' temporalmente
    if exists(data_dir):
        rmtree(data_dir)
    copytree(config_dir, data_dir)

env.AddPreAction("buildfs", before_buildfs)
