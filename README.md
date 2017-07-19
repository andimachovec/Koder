# **![Koder icon](./artwork/Koder_icon_64.png)** Koder

Koder is a code editor built using Scintilla editing component, inspired by Notepad++.

![Screenshot](./artwork/Koder.png)

## Dependencies

* Scintilla
* yaml-cpp

## Building

Koder uses makefile_engine. Invoke make in root directory.

## Installation

Copy all files from **data** directory to **Koder** directory in any non-packaged/data folder.

## History

0.3.0 - *TBD*

* New icon.
* Add right-click menu.
* Disable menu items according to editor state.
* Fix multiple language entries in Language menu.
* Show alert if styles not found.
* Change shortcut for Go to line.
* Add View->Wrap lines.
* Add File->Reload.
* Add tooltips to toolbar buttons.
* Add option to attach windows to existing ones.
* Cascade windows if necessary.
* Open files in the same window only if it is new window and has not been modified.
* Add option to highlight trailing whitespace.
* Add option to trim trailing whitespace.
* Add option to trim trailing whitespace automatically on save.
* Use multiline controls in Find window.

0.2.0 - *15-04-2017*

* Add icon.
* Add translations.
* Adjust whitespace size.
* Fix crash when changing git branches with a file open.
* Fix horizontal scrollbar on long lines.
* Recolor the document on language change.
* Handle drag and drop message.
* Add jumping to specific position in file from command line. Handle parameters from /bin/open.
* Add comment line/block feature.
* Add indent guides highlighting.
* Add editor style menu to preferences.
* Add toolbar.
* Put selection in Find window automatically.
* Add AWK, Perl, Ruby and Rust support.

0.1.0 - *01-01-2017*

* Initial release.