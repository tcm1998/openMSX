# $Id$
#
# Some notes about static linking:
# There are two ways of linking to static library: using the -l command line
# option or specifying the full path to the library file as one of the inputs.
# When using the -l option, the library search paths will be searched for a
# dynamic version of the library, if that is not found, the search paths will
# be searched for a static version of the library. This means we cannot force
# static linking of a library this way. It is possible to force static linking
# of all libraries, but we want to control it per library.
# Conclusion: We have to specify the full path to each library that should be
#             linked statically.

class Library(object):
	libName = None
	makeName = None
	header = None
	configScriptName = None
	dynamicLibsOption = '--libs'
	staticLibsOption = None
	function = None
	dependsOn = ()

	@classmethod
	def isSystemLibrary(cls, platform): # pylint: disable-msg=W0613
		'''Returns True iff this library is a system library on the given
		platform.
		A system library is a library that is available systemwide in the
		minimal installation of the OS.
		The default implementation returns False.
		'''
		return False

	@classmethod
	def getConfigScript( # pylint: disable-msg=W0613
		cls, platform, linkStatic, distroRoot
		):
		scriptName = cls.configScriptName
		if scriptName is None:
			return None
		elif distroRoot is None:
			return scriptName
		else:
			return '%s/bin/%s' % (distroRoot, scriptName)

	@classmethod
	def getHeaders(cls, platform): # pylint: disable-msg=W0613
		header = cls.header
		return header if hasattr(header, '__iter__') else (header, )

	@classmethod
	def getLibName(cls, platform): # pylint: disable-msg=W0613
		return cls.libName

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		if configScript is not None:
			flags = [ '`%s --cflags`' % configScript ]
		elif distroRoot is None or cls.isSystemLibrary(platform):
			flags = []
		else:
			flags = [ '-I%s/include' % distroRoot ]
		dependentFlags = [
			librariesByName[name].getCompileFlags(
				platform, linkStatic, distroRoot
				)
			for name in cls.dependsOn
			]
		return ' '.join(flags + dependentFlags)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		if configScript is not None:
			libsOption = (
				cls.dynamicLibsOption
				if not linkStatic or cls.isSystemLibrary(platform)
				else cls.staticLibsOption
				)
			if libsOption is not None:
				return '`%s %s`' % (configScript, libsOption)
		if distroRoot is None or cls.isSystemLibrary(platform):
			return '-l%s' % cls.getLibName(platform)
		else:
			flags = [
				'%s/lib/lib%s.a' % (distroRoot, cls.getLibName(platform))
				] if linkStatic else [
				'-L%s/lib -l%s' % (distroRoot, cls.getLibName(platform))
				]
			dependentFlags = [
				librariesByName[name].getLinkFlags(
					platform, linkStatic, distroRoot
					)
				for name in cls.dependsOn
				]
			return ' '.join(flags + dependentFlags)

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		'''Returns the version of this library, "unknown" if there is no
		mechanism to retrieve the version, None if there is a mechanism
		to retrieve the version but it failed, or a callable that takes a
		CompileCommand and a log stream as its arguments and returns the
		version or None if retrieval failed.
		'''
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		if configScript is None:
			return 'unknown'
		else:
			return '`%s --version`' % configScript

class FreeType(Library):
	libName = 'freetype'
	makeName = 'FREETYPE'
	header = ('<ft2build.h>', 'FT_FREETYPE_H')
	configScriptName = 'freetype-config'
	function = 'FT_Open_Face'
	dependsOn = ('ZLIB', )

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		return '`%s --ftversion`' % configScript

class GL(Library):
	libName = 'GL'
	makeName = 'GL'
	function = 'glGenTextures'

	@classmethod
	def isSystemLibrary(cls, platform):
		return True

	@classmethod
	def getHeaders(cls, platform):
		if platform == 'darwin':
			return ('<OpenGL/gl.h>', )
		else:
			return ('<GL/gl.h>', )

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		if platform in ('netbsd', 'openbsd'):
			return '-I/usr/X11R6/include -I/usr/X11R7/include'
		else:
			return super(GL, cls).getCompileFlags(
				platform, linkStatic, distroRoot
				)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		if platform == 'darwin':
			return '-framework OpenGL'
		elif platform == 'mingw32':
			return '-lopengl32'
		elif platform in ('netbsd', 'openbsd'):
			return '-L/usr/X11R6/lib -L/usr/X11R7/lib -lGL'
		else:
			return super(GL, cls).getLinkFlags(platform, linkStatic, distroRoot)

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			versionPairs = tuple(
				( major, minor )
				for major in range(1, 10)
				for minor in range(0, 10)
				)
			version = cmd.expand(log, cls.getHeaders(platform), *(
				'GL_VERSION_%d_%d' % pair for pair in versionPairs
				))
			try:
				return '%d.%d' % max(
					ver
					for ver, exp in zip(versionPairs, version)
					if exp is not None
					)
			except ValueError:
				return None
		return execute

class GLEW(Library):
	makeName = 'GLEW'
	header = '<GL/glew.h>'
	function = 'glewInit'
	dependsOn = ('GL', )

	@classmethod
	def getLibName(cls, platform):
		if platform == 'mingw32':
			return 'glew32'
		else:
			return 'GLEW'

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		flags = super(GLEW, cls).getCompileFlags(
			platform, linkStatic, distroRoot
			)
		if platform == 'mingw32' and linkStatic:
			return '%s -DGLEW_STATIC' % flags
		else:
			return flags

class JACK(Library):
	libName = 'jack'
	makeName = 'JACK'
	header = '<jack/jack.h>'
	function = 'jack_client_new'

class LibPNG(Library):
	libName = 'png12'
	makeName = 'PNG'
	header = '<png.h>'
	configScriptName = 'libpng-config'
	dynamicLibsOption = '--ldflags'
	function = 'png_write_image'
	dependsOn = ('ZLIB', )

class LibXML2(Library):
	libName = 'xml2'
	makeName = 'XML'
	header = '<libxml/parser.h>'
	configScriptName = 'xml2-config'
	function = 'xmlSAXUserParseFile'
	dependsOn = ('ZLIB', )

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform == 'darwin'

	@classmethod
	def getConfigScript(cls, platform, linkStatic, distroRoot):
		if platform == 'darwin':
			# Use xml2-config from /usr: ideally we would use xml2-config from
			# the SDK, but the SDK doesn't contain that file. The -isysroot
			# compiler argument makes sure the headers are taken from the SDK
			# though.
			return '/usr/bin/%s' % cls.configScriptName
		else:
			return super(LibXML2, cls).getConfigScript(
				platform, linkStatic, distroRoot
				)

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		flags = super(LibXML2, cls).getCompileFlags(
			platform, linkStatic, distroRoot
			)
		if not linkStatic or cls.isSystemLibrary(platform):
			return flags
		else:
			return flags + ' -DLIBXML_STATIC'

class SDL(Library):
	libName = 'SDL'
	makeName = 'SDL'
	header = '<SDL.h>'
	configScriptName = 'sdl-config'
	staticLibsOption = '--static-libs'
	function = 'SDL_Init'

class SDL_image(Library):
	libName = 'SDL_image'
	makeName = 'SDL_IMAGE'
	header = '<SDL_image.h>'
	function = 'IMG_LoadPNG_RW'
	dependsOn = ('SDL', 'PNG')

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			version = cmd.expand(log, cls.getHeaders(platform),
				'SDL_IMAGE_MAJOR_VERSION',
				'SDL_IMAGE_MINOR_VERSION',
				'SDL_IMAGE_PATCHLEVEL',
				)
			return None if None in version else '%s.%s.%s' % version
		return execute

class SDL_ttf(Library):
	libName = 'SDL_ttf'
	makeName = 'SDL_TTF'
	header = '<SDL_ttf.h>'
	function = 'TTF_OpenFont'
	dependsOn = ('SDL', 'FREETYPE')

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			version = cmd.expand(log, cls.getHeaders(platform),
				'SDL_TTF_MAJOR_VERSION',
				'SDL_TTF_MINOR_VERSION',
				'SDL_TTF_PATCHLEVEL',
				)
			return None if None in version else '%s.%s.%s' % version
		return execute

class TCL(Library):
	libName = 'tcl'
	makeName = 'TCL'
	header = '<tcl.h>'
	function = 'Tcl_CreateInterp'
	staticLibsOption = '--static-libs'

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform == 'darwin'

	@classmethod
	def getConfigScript(cls, platform, linkStatic, distroRoot):
		if distroRoot is None or cls.isSystemLibrary(platform):
			return 'build/tcl-search.sh'
		else:
			return 'TCL_CONFIG_DIR=%s/lib build/tcl-search.sh' % distroRoot

class ZLib(Library):
	libName = 'z'
	makeName = 'ZLIB'
	header = '<zlib.h>'
	function = 'inflate'

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform == 'darwin'

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			version = cmd.expand(log, cls.getHeaders(platform), 'ZLIB_VERSION')
			return None if version is None else version.strip('"')
		return execute

# Build a dictionary of libraries using introspection.
def _discoverLibraries(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, Library):
			if not (obj is Library):
				yield obj.makeName, obj
librariesByName = dict(_discoverLibraries(locals().itervalues()))