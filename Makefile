APP = vfs_rar
DEBUG ?= 0

OBJDIR = .objs
DEPDIR = .deps

UNRARDIR = unrar

CXX ?= g++
STRIP = strip

CXXFLAGS += -fPIC
CXXFLAGS += -Wall \
	    -Wno-parentheses \
	    -Wno-switch \
	    -Wno-sign-compare \
	    -Wno-unused-variable -Wno-unused-function \
	    -Wno-maybe-uninitialized
CXXFLAGS += -I. -I./unrar
CXXFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DRAR_SMP
ifeq ($(DEBUG),1)
CXXFLAGS += -g -DDEBUG
endif

LDFLAGS = -pthread

BIN = $(APP).so

UNRAR_OBJS = filestr.o recvol.o rs.o scantree.o qopen.o
UNRAR_OBJS += strlist.o \
	      strfn.o \
	      pathfn.o \
	      smallfn.o \
	      global.o \
	      file.o \
	      filefn.o \
	      filcreat.o \
	      archive.o \
	      arcread.o \
	      unicode.o \
	      system.o \
	      isnt.o \
	      crypt.o \
	      crc.o \
	      rawread.o \
	      encname.o \
	      resource.o \
	      match.o \
	      timefn.o \
	      rdwrfn.o \
	      consio.o \
	      options.o \
	      errhnd.o \
	      rarvm.o \
	      secpassword.o \
	      rijndael.o \
	      getbits.o \
	      sha1.o \
	      sha256.o \
	      blake2s.o \
	      hash.o \
	      extinfo.o \
	      extract.o \
	      volume.o \
	      list.o \
	      find.o \
	      unpack.o \
	      headers.o \
	      threadpool.o \
	      rs16.o \
	      cmddata.o \
	      ui.o

OBJS = $(UNRAR_OBJS) $(APP).o

vpath %.cpp $(UNRARDIR)
vpath %.hpp $(UNRARDIR)

.PHONY: all
all: $(BIN)

$(BIN): $(addprefix $(OBJDIR)/,$(OBJS))
	$(CXX) -shared $(CXXFLAGS) -o $@ $^ $(CXX_LDFLAGS) $(LDFLAGS)
ifneq ($(DEBUG),1)
	$(STRIP) $@
endif

vpath %.o $(OBJDIR)
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c $< -o $@
$(OBJDIR):
	-mkdir -p $@

vpath %.d $(DEPDIR)
$(DEPDIR)/%.d: %.cpp | $(DEPDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c -MM $< > $@
	sed -i 's,\($*\)\.o[ :]*,\1.o $*.d: ,g' $@
$(DEPDIR):
	-mkdir -p $@

-include $(addprefix $(DEPDIR)/,$(OBJS:.o=.d))

.PHONY: install uninstall clean
install: $(BIN)
	-mkdir -p ~/.local/lib/deadbeef
	-cp $(BIN) ~/.local/lib/deadbeef

uninstall:
	-rm -rf ~/.local/lib/deadbeef/$(BIN)

clean:
	-rm -rf $(BIN) $(OBJDIR) $(DEPDIR)

