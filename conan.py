from conans import ConanFile

class App(ConanFile):
    settings = ("os", "arch", "compiler", "build_type")
    requires = ("frozen/1.0.1",)

