PROJECT_NAME = tb

CC = cl

# build mode
!ifndef BUILD
BUILD = release
!endif

# build flags
!if "$(BUILD)" == "debug"
!MESSAGE - $(PROJECT_NAME): building in DEBUG mode
CFLAGS = $(CFLAGS) /Od /Ob0 /Zi /MDd /DDEBUG
!else
!MESSAGE - $(PROJECT_NAME): building in RELEASE mode
CFLAGS = $(CFLAGS) /O2 /Ob2 /DNDEBUG /MD
!endif

# more warnings
CFLAGS = $(CFLAGS) /W3 /WX /D_CRT_SECURE_NO_WARNINGS

# default MSVC flags
CFLAGS = $(CFLAGS) /nologo /DWIN32 /D_WINDOWS /EHsc

$(PROJECT_NAME): main.c
	@if not exist build mkdir build
	$(CC) $(CFLAGS) main.c /Fd"build\\vc.pdb" /Fo"build\\main.obj" /Fe"build\\$(PROJECT_NAME).exe"

