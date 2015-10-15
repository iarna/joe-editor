
import time
import joefx

### TODO: Move test classes into individual files as they get bigger

# Invoke with: python3 -m unittest alltests

class FindTests(joefx.JoeTestBase):
    def test_abracadabra(self):
        """Regression test for #295: infinite loop on search-and-replace"""
        self.startJoe()
        self.write("abracadabra")
        self.cmd("bol")
        self.replace("a", "X")
        self.answerReplace("yyy")
        self.writectl("^C")
        self.joe.writectl("{left*2}{bs}a")
        self.cmd("bol,fnext")
        self.answerReplace("yy")
        self.writectl("^C")
        self.assertTextAt("XbrXcXdXbra", x=0)
    
    def test_replace_zerolen_regex(self):
        """Regression test related to #295.  Check original functionality that broke"""
        self.workdir.fixtureData("test", "line 1\nline 2\nline 3\nline 4\n")
        self.startup.args = ("test",)
        self.startJoe()
        self.replace(r"\$", "")
        self.assertCursor(y=1)
        self.write("y")
        self.assertCursor(y=2)
        self.write("y")
        self.assertCursor(y=3)
        self.write("y")
        self.assertCursor(y=4)
        self.write("y")
        self.assertCursor(x=0, y=5)
    
    def test_replace_skip_zerolen_regex(self):
        """Regression test related to #295.  Check original functionality that broke"""
        self.workdir.fixtureData("test", "line 1\nline 2\nline 3\nline 4\n")
        self.startup.args = ("test",)
        self.startJoe()
        
        self.replace(r"\$", "")
        self.assertCursor(y=1)
        self.write("n")
        self.assertCursor(y=2)
        self.write("n")
        self.assertCursor(y=3)
        self.write("n")
        self.assertCursor(y=4)
        self.write("n")
        self.assertCursor(x=0, y=5)

class WindowTests(joefx.JoeTestBase):
    def test_showlog_querysave(self):
        """Regression test for #326: joe locks up if you try querysave on the startup log"""
        self.startJoe()
        
        self.cmd("showlog,nextw,abort,querysave")
        self.joe.flushin()
        self.cmd("abort")
        self.assertExited()
    
    def test_resize_menu_narrower_doesnt_crash(self):
        """Regression test for #310: Segfault when resizing with ^T menu open"""
        self.startup.lines = 25
        self.startup.columns = 120
        self.startJoe()
        
        # Pull up menu and resize down to 40
        self.menu("root")
        
        for i in range(119, 39, -1):
            self.joe.resize(i, 25)
            self.joe.flushin()
        
        self.writectl("^C")
        self.cmd("abort")
        self.assertExited()
    
    def test_resize_menu_wider_doesnt_crash(self):
        """Regression test for #310: Segfault when resizing with ^T menu open"""
        self.startup.lines = 25
        self.startup.columns = 80
        self.startJoe()
        
        self.joe.resize(40, 25)
        
        # Pull up menu and resize up to 120
        self.menu("root")
        
        for i in range(40, 121):
            self.joe.resize(i, 25)
            self.joe.flushin()
        
        self.writectl("^C")
        self.cmd("abort")
        self.assertExited()

class EncodingTests(joefx.JoeTestBase):
    def test_hex_mode_and_back(self):
        """Regresion test.  Was not listed as a bug, but was fixed with [11618ed3b38f]"""
        self.workdir.fixtureData("test", "ร้อนนี้คงไม่มีอะไรสำคัญเท่าครีมกันแดดอีกแล้วนะสาวๆ")
        self.startup.args = ("test",)
        self.startJoe()
	
        self.writectl("{right*21}")
        self.assertCursor(16, 1)
        
        self.mode("hex")
        
        # Check cursor movement
        self.assertCursor(75, 4)
        self.writectl("{left}")
        self.assertCursor(74, 4)
        self.writectl("{right}")
        self.assertCursor(75, 4)
        
        self.mode("hex")

        # Check cursor movement
        self.assertCursor(16, 1)
        self.writectl("{left}")
        self.assertCursor(15, 1)
    
    def test_change_updates_cursor(self):
        """Regresion test.  Was not listed as a bug, but was fixed with [11618ed3b38f]"""
        self.workdir.fixtureData("test", "ร้อนนี้คงไม่มีอะไรสำคัญเท่าครีมกันแดดอีกแล้วนะสาวๆ")
        self.startup.args = ("test",)
        self.startJoe()
        
        self.writectl("{right*21}")
        self.assertCursor(16, 1)
        
        # Change encoding to ascii
        self.encoding("ascii")
        
        # Check cursor movement
        self.assertCursor(63, 1)
        self.writectl("{left}")
        self.assertCursor(62, 1)
        self.writectl("{right}")
        self.assertCursor(63, 1)
        
        # Back to utf-8
        self.encoding("utf-8")
        
        # Check cursor movement
        self.assertCursor(16, 1)
        self.writectl("{left}")
        self.assertCursor(15, 1)
    
    def test_utf16le_detection(self):
        """Tests UTF-16 detection + proper save with a UTF-16LE file"""
        self.config.globalopts.guess_utf16 = True
        self.workdir.fixtureData("test", "This is a UTF-16 file\n".encode('utf-16le'))
        self.startup.args = ("test",)
        self.startJoe()
        
        self.assertTextAt("This is a UTF-16 file")
        self.writectl("{down}Extra line{enter}")
        self.save()
        self.cmd("abort")
        self.assertExited()
        
        self.assertFileContents("test", "This is a UTF-16 file\nExtra line\n".encode('utf-16le'))
    
    def test_utf16be_detection(self):
        """Tests UTF-16 detection + proper save with a UTF-16BE file"""
        self.config.globalopts.guess_utf16 = True
        self.workdir.fixtureData("test", "This is a UTF-16 file\n".encode('utf-16be'))
        self.startup.args = ("test",)
        self.startJoe()
        
        self.assertTextAt("This is a UTF-16 file")
        self.writectl("{down}Extra line{enter}")
        self.save()
        self.cmd("abort")
        self.assertExited()
        
        self.assertFileContents("test", "This is a UTF-16 file\nExtra line\n".encode('utf-16be'))
    
    def test_utf16_detection_off(self):
        """Tests that UTF-16 detection is not performend when not enabled through joerc (+ test sanity check)"""
        self.config.globalopts.guess_utf16 = False
        self.workdir.fixtureData("test", "This is a UTF-16 file\n".encode('utf-16le'))
        self.startup.args = ("test",)
        self.startJoe()
        
        self.assertTextAt("T@h@i@s@ @i@s@ @a@ @U@T@F@-@1@6@ @f@i@l@e@")

class FormatTests(joefx.JoeTestBase):
    def test_wordwrap_with_asterisks(self):
        """Regression test for #265: Version 3.7 treats "-" and "*" as quote characters"""
        self.maxDiff = None # Show whole string on failure
        self.config.globalopts.wordwrap = True
        self.config.globalopts.autoindent = False
        self.config.globalopts.pastehack = False
        self.startJoe()
        
        self.write("*all* work and no play makes jack a dull boy. all work and no play makes jack a dull boy. all work and no play makes jack a dull boy. all work and no play makes jack a dull boy.")
        self.save("testout")
        self.cmd("abort")
        self.assertExited()
        self.assertFileContents("testout", "*all* work and no play makes jack a dull boy. all work and no play makes\njack a dull boy. all work and no play makes jack a dull boy. all work and no\nplay makes jack a dull boy.")
