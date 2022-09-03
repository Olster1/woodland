# Woodland Editor

Aimed to be a text editor that:
1. Loads Instantly on startup

2. Load and edit big files just as easily as small files

3. A single exe file

4. Stable - doesn't crash

5. A cross between Sublime Text and 4Coder

It's really just for me as a fun project, something to practice using Direct3d and practice getting better at programming :)

## Roadmap
- [x] Render text using d3d 11
- [x] Using Gap Buffer for text editing 
- [x] Open file using Windows Dialog Box
- [x] Save file using Windows Dialog Box
- [x] Drag and Drop a file in
- [x] don't render utf-8 BOM at start of file
- [x] View multiple buffers open and jump between the panels
- [x] Scroll buffer and jump to cursor when active
- [x] Keyword hightlighting of C/C++ keywords
- [x] Copy and Paste from the Clipboard
- [x] Mouse click to move cursor in the buffer
- [x] Ctrl - Arrow to jump tokens back and forward 
- [x] Saves windows size from last session in AppData folder

## Near Future (roughly in order of priority)
- [x] Up and down arrows to move line by line
- [x] End of line key & home key to jump to end of line and start respectively
- [x] Open buffer that has been loaded (ctrl - b -> open a dropdown of all open buffers)
- [x] Add search filter for open buffers
- [x] Robust Undo/Redo 
- [x] Ctrl-Find in an open buffer (not fuzzy search)
- [ ] Shift highlighting of multiple lines to cut, copy & delete text - both with Shift-Arrows and Mouse Click-Drag


- [ ] Ctrl-Find in a whole project (not fuzzy search)
- [ ] Recomend code based on what's in the buffer already (drop down of choices (is fuzzy) that you can choose with arrow keys, like sublime text)
- [ ] Ctrl-G jump to line in buffer
- [ ] Error highlightling when brackets or parenthesis don't match
- [ ] Vertical Guidlines to match up scope levels 

- [ ] Chunk the buffer to optimize editing and rendering the buffer
- [ ] Full Unicode support (trying to account for it as I go, but not fully and sure to be mistakes and edge cases not accounting for)
- [ ] Utf-16 rendering (have to find a way of detecting if it is a utf-16 file - maybe just look for the BOM header, also have to write a utf-16 decoded which doesn't seem to hard)
- [ ] Have concept of project that is remembered (just a folder, store previous projects in AppData)
- [ ] More intuitive GUI than 4Coder for choosing a open buffer and navigating a project (more like sublime text)
- [ ] Detect when a file should just be rendered as hex code (binary files) instead of text
- [ ] Choose Color themes and build your own easily with GUI color picker that changes text in real-time


## Longer Support
- [ ] Multiple Active Cursors in a buffer
- [ ] More encodings than utf-8,hex & utf-16. For example Greek (Windows 1253), Celtic (ISO-8859-14)
- [ ] More Syntax highlighting for different languages
- [ ] Add build system with jump-to error 
- [ ] Record macros like emacs
- [ ] Beautify C/C++



## If you encounter a crash bug: 

Go to %UserName%/AppData/Local/CrashDumps and get the crash file that corresponds to your crash. Please send to ollietheexplorer@gmail.com