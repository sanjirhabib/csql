## csql

csql is terminal based sqlite3 database viewer and eidtor.

### Features

- Spreadsheet like browsing.
- In cell editing and adding.
- Table structure editing.

### Installation

```bash
git clone --depth=1 https://github.com/sanjirhabib/csql
make install
```

To uninstall
```bash
make uninstall
```

### Usage
csql database.db

Key bindings 
    ESC to go back.
    Enter a cell to edit
    Enter title of table/menu for more menu

    Keys are windows binded when you are in edit mode.
    When you are in view mode, browsing the table, vim key codes work along with windows too.
