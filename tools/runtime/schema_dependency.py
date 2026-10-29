"""Pinned build-only Schema headers and the external C++ ABI bridge."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
ROOT=Path(__file__).resolve().parents[2]
VALIJSON='af2383509277f8e8051acbb3e4b3060f4779aa10'
VALIJSON_SHA='8ed143b6c85056b140b627483f36f90353b62990d2274326c2e517104272aceb'
JSON='55f93686c01528224f448c19128836e7df245f72'
JSON_SHA='aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63'

def includes():
    cache=Path(os.environ.get('XDG_CACHE_HOME',Path.home()/'.cache'))/'pangu/schema-deps'
    cache.mkdir(parents=True,exist_ok=True)
    def fetch(name,url,digest):
        target=cache/name
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=digest:
            with urllib.request.urlopen(url,timeout=60) as response:
                data=response.read()
            if hashlib.sha256(data).hexdigest()!=digest:
                raise ValueError('dependency digest mismatch: '+name)
            temporary=target.with_suffix('.download');temporary.write_bytes(data);temporary.replace(target)
        return target
    archive=fetch('valijson-'+VALIJSON+'.tar.gz','https://codeload.github.com/tristanpenman/valijson/tar.gz/'+VALIJSON,VALIJSON_SHA)
    header=fetch('nlohmann-'+JSON+'.hpp','https://raw.githubusercontent.com/nlohmann/json/'+JSON+'/single_include/nlohmann/json.hpp',JSON_SHA)
    directory=cache/VALIJSON
    if not (directory/'include/valijson/schema.hpp').exists():
        with tarfile.open(archive) as source:
            for member in source.getmembers():
                parts=Path(member.name).parts
                if member.isfile() and len(parts)>3 and parts[1:3]==('include','valijson') and '..' not in parts:
                    target=directory/Path(*parts[1:]);target.parent.mkdir(parents=True,exist_ok=True)
                    with source.extractfile(member) as data:
                        target.write_bytes(data.read())
    target=directory/'include/nlohmann/json.hpp';target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=JSON_SHA:
        shutil.copyfile(header,target)
    return directory/'include'

def compile_inputs(sources,directory,sanitize=()):
    result=[];cpp=False
    for source in sources:
        path=Path(source)
        if not path.is_absolute():path=ROOT/path
        if path.suffix=='.cpp':
            output=Path(directory)/(path.stem+'.o')
            flags=subprocess.check_output(['pkg-config','--cflags','json-c'],text=True).split()
            command=['/usr/bin/clang-23','-std=c++17','-O2','-Wall','-Wextra','-Werror','-pedantic','-DVALIJSON_USE_EXCEPTIONS=1','-isystem',str(includes()),*sanitize,*flags]
            compiler=subprocess.check_output(['/usr/bin/clang-23','--version'])
            identity=hashlib.sha256(path.read_bytes()+path.with_suffix('.h').read_bytes()+compiler+repr(command).encode()+VALIJSON_SHA.encode()+JSON_SHA.encode()).hexdigest()
            cache=includes().parent/'objects';cache.mkdir(exist_ok=True)
            cached=cache/(identity+'.o')
            if not cached.exists():
                with tempfile.TemporaryDirectory(dir=cache) as temporary:
                    built=Path(temporary)/'bridge.o'
                    subprocess.run([*command,'-c',str(path),'-o',str(built)],check=True)
                    built.replace(cached)
            shutil.copyfile(cached,output)
            result.append(str(output));cpp=True
        else:result.append(str(path))
    return result+(['-lstdc++','-lm'] if cpp else [])
