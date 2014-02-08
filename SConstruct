env = Environment(YACCFLAGS = '-d', CCFLAGS = '-std=c++11 -Wall')
env.ParseConfig('llvm-config --cppflags --libs --ldflags core jit native')
env.Append(LINKFLAGS = ['-rdynamic'])
env.Append(LIBS = ['dl', 'pthread'])

lexer = env.CXXFile(target = 'kaleidoscope.ll.cpp', source = 'kaleidoscope.ll')
parser = env.CXXFile(target = 'kaleidoscope.yy.cpp', source = 'kaleidoscope.yy')

sources = [lexer[0]]
sources.append(parser[0])

for src in Glob("*.cpp"):
  if src not in sources:
    sources.append(src)

env.Program('test', sources)

