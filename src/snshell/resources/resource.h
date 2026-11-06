/* Resource ids, shared by snshell.rc, snowball.rc and the code. The icons,
   bitmaps and cursors (IDI_, IDB_, IDR_) are W98ART.DLL's: the same numbers
   as ..\w98icon\src\w98art.h, kept in step with it. */
#ifndef RESOURCE_H
#define RESOURCE_H

/* Icons */
#define IDI_SNOWBALL    1
#define IDI_COMPUTER    2
#define IDI_FOLDER      3
#define IDI_FILE        4
#define IDI_PROGRAM     5
#define IDI_FLOPPY      6
#define IDI_HARDDISK    7
#define IDI_CDROM       8
#define IDI_NETWORK     9
#define IDI_RAMDISK     10
#define IDI_BIN         11
#define IDI_RUN         12
#define IDI_SHUTDOWN    13
#define IDI_SM_PROGRAMS 14
#define IDI_SM_DOCS     15
#define IDI_SM_SETTINGS 16
#define IDI_SM_FIND     17
#define IDI_SM_HELP     18
#define IDI_SM_RUN      19
#define IDI_SM_SHUTDOWN 20
#define IDI_BINFULL     21
#define IDI_EXPLORER    22
#define IDI_ARROW       23      /* the shortcut arrow, drawn over icons */
#define IDI_SHOWDESKTOP 24      /* the Desktop: Show Desktop, Desktop as Shortcut */
#define IDI_MYDOCS      25      /* My Documents (MYDOCS.DLL's) */
#define IDI_LAST        IDI_MYDOCS
/* W98ART.DLL's other icons, ids 26 on (art\W98ART.TXT): those Snowball shows */
#define IDI_DISPLAY     30      /* DESK.CPL's monitor: the Theme Manager */
#define IDI_RESMETER    240     /* RSRCMTR.EXE's */
#define IDI_METER       241     /* ... its tray icon full; + 1 to 12 lower, + 12 empty */
#define IDI_MUTED       257     /* SNDVOL32.EXE's tray speaker, muted */
#define IDI_SPEAKER     258
#define IDI_ART_LAST    258
/* The network's tray icons, in SNSHELL.DLL; IDLE + 1 sending, + 2 receiving, + 3 both */
#define IDI_NETIDLE     300
#define IDI_NETSEND     301
#define IDI_NETRECV     302
#define IDI_NETBOTH     303
#define IDI_NETDOWN     304
#define IDI_NETWARN     305

/* Bitmaps */
#define IDB_STARTFLAG   100     /* the Start button's 16x16 flag, magenta = transparent */
#define IDB_TOOLBAR     102     /* Open and Save As: TBI_* images, magenta = transparent */
#define IDB_TOOLBAROFF  103     /* the same greyed */
#define IDB_FOLDERBAR   104     /* folder windows' Standard Buttons: FBI_* images */
#define IDB_FOLDERBARCOLD 105   /* the same grey, until the mouse comes */
#define IDB_FOLDERBAROFF 106    /* the same greyed */
#define IDB_MONITOR     107     /* Display Properties' monitor, its screen at (16,17) 152x112 */
#define FBI_BACK        0
#define FBI_FORWARD     1
#define FBI_UP          2
#define FBI_CUT         3
#define FBI_COPY        4
#define FBI_PASTE       5
#define FBI_DELETE      6
#define FBI_PROPERTIES  7
#define FBI_VIEWS       8
#define TBI_UP          0
#define TBI_NEWFOLDER   1
#define TBI_LIST        2
#define TBI_DETAILS     3


/* Drag and drop cursors */
#define IDR_DRAGMOVE    110
#define IDR_DRAGCOPY    111
#define IDR_DRAGLINK    112
#define IDR_NODROP      113

/* Dialogs and their controls */
#define IDD_RUN         200
#define IDD_SHUTDOWN    201
#define IDD_ABOUT       202
#define IDD_PROGRESS    203
#define IDD_REPLACE     204
#define IDD_PROPFILE    205     /* files and folders: General, and Shortcut for .SHC */
#define IDD_PROPDRIVE   206
#define IDD_PROPSYSTEM  207     /* My Computer */
#define IDD_CHANGEICON  208
#define IDD_GOTO        209
#define IDD_FIND        210
#define IDD_TASKMAN     211
#define IDD_ENDTASK     212     /* "This program is not responding" */
#define IDD_OPENSAVE    213     /* every program's Open and Save As */
#define IDD_TBPROPS     214     /* Taskbar Properties */
#define IDD_NEWSHORTCUT 215     /* its Add: the Create Shortcut wizard */
#define IDD_REMOVESC    216     /* its Remove: Remove Shortcuts/Folders */
#define IDD_FOLDEROPTS  217     /* Folder Options */
#define IDD_PROMPT      218     /* a name asked for: Rename (Start menu), Save Scheme */
#define IDD_DISPLAY     219     /* Display Properties */
#define IDD_OPENWITH    220     /* Open With */
#define IDD_FILETYPE    221     /* Edit File Type, Add New File Type */
#define IDD_FILEACTION  222     /* its New Action, Edit Action */
#define IDD_BINPROPS    223     /* Recycle Bin Properties */
#define IDD_VOLUME      224     /* the tray's volume slider */
#define IDD_RESMETER    225     /* Resource Meter */
#define IDD_FORMAT      226     /* Format (a floppy) */
#define IDD_CLEANUP     227     /* Disk Cleanup */
#define IDD_LOWDISK     228     /* the low disk space warning */
#define IDD_DATETIME    229     /* Date/Time Properties */
#define IDD_STARTSIZE   230     /* Customize Start Menu (the two-column one's size) */
#define IDD_THEMES      231     /* Theme Manager */
#define IDD_NETSTATUS   232     /* the network's status window */
#define IDD_NETPROPS    233     /* its settings */

#define IDC_COMMAND     300
#define IDC_BROWSE      301
#define IDC_SHUTDOWN    302
#define IDC_RESTART     303
#define IDC_TODOS       304
#define IDC_VERSION     305
#define IDC_INFO        306
#define IDC_TEXT        310     /* replace: question */
#define IDC_OLDICON     311
#define IDC_OLDINFO     312
#define IDC_NEWICON     313
#define IDC_NEWINFO     314
#define IDC_YESALL      315
#define IDC_DLGICON     316     /* a dialog's own icon (Run, Shut Down, Replace), set in code */
#define IDC_TABS        400
#define IDC_APPLY       401
#define IDC_CHANGEICON_FILE      410     /* Change Icon */
#define IDC_CHANGEICON_BROWSE    411
#define IDC_CHANGEICON_LIST      412
#define IDC_SMC_ICON       414     /* Customize Start Menu */
#define IDC_SMC_COUNT      415     /* [Taskbar] MostUsed */
#define IDC_SMC_CLEAR      416
#define IDC_SMC_WIDTH      417     /* [Taskbar] MenuWidth */
#define IDC_TM_LISTL       660     /* Theme Manager: the list and its buttons */
#define IDC_TM_LIST        661
#define IDC_TM_NEW         662
#define IDC_TM_COPY        663
#define IDC_TM_RENAME      664
#define IDC_TM_DELETE      665
#define IDC_TM_GROUP       666
#define IDC_TM_IMPORT      667
#define IDC_TM_EXPORT      668
#define IDC_TM_DEFAULTS    669
#define IDC_TM_PARTS       670     /* the parts to apply: */
#define IDC_TM_PART        671     /* ..676, a check box per THEME_* bit */
#define IDC_GOTO_PATH      420     /* Go To Folder */
#define IDC_PROMPT_LABEL   424     /* a name asked for */
#define IDC_PROMPT_TEXT    425
#define IDC_FIND_NAMED     430     /* Find */
#define IDC_FIND_TEXT      431
#define IDC_FIND_LOOKIN    432
#define IDC_FIND_SUBFOLDERS 433
#define IDC_FIND_FIND      434
#define IDC_FIND_STOP      435
#define IDC_FIND_NEW       436
#define IDC_TASKMAN_LIST      440     /* Task Manager: Applications */
#define IDC_TASKMAN_SWITCH    442
#define IDC_TASKMAN_END       443
#define IDC_TASKMAN_NEW       444
#define IDC_TASKMAN_PROCS     445     /* Processes */
#define IDC_TASKMAN_LIBS      446
#define IDC_TASKMAN_KILL      447
#define IDC_OPENSAVE_LOOKINL   480     /* Open and Save As */
#define IDC_OPENSAVE_LOOKIN    481
#define IDC_OPENSAVE_UP        482     /* the toolbar's commands, in TBI_* order */
#define IDC_OPENSAVE_NEWFOLDER 483
#define IDC_OPENSAVE_LIST      484
#define IDC_OPENSAVE_DETAILS   485
#define IDC_OPENSAVE_VIEW      486
#define IDC_OPENSAVE_NAMEL     487
#define IDC_OPENSAVE_NAME      488
#define IDC_OPENSAVE_TYPEL     489
#define IDC_OPENSAVE_TYPE      490
#define IDC_OPENSAVE_READONLY  491
#define IDC_OPENSAVE_HELP      492
#define IDC_OPENSAVE_TOOLBAR   493
#define IDC_WIZARD_PICTURE   500     /* Create Shortcut */
#define IDC_WIZARD_BACK      501
#define IDC_WIZARD_NEXT      502
#define IDC_REMOVE_TREE      503     /* Remove Shortcuts/Folders */
#define IDC_REMOVE_REMOVE    504
#define IDC_OPENWITH_TEXT    510     /* Open With */
#define IDC_OPENWITH_DESCL   511
#define IDC_OPENWITH_DESC    512
#define IDC_OPENWITH_LIST    513
#define IDC_OPENWITH_ALWAYS  514
#define IDC_OPENWITH_OTHER   515
#define IDC_FILETYPE_ICON    520     /* Edit File Type */
#define IDC_FILETYPE_CHANGEICON 521
#define IDC_FILETYPE_DESC    522
#define IDC_FILETYPE_EXTL    523
#define IDC_FILETYPE_EXT     524
#define IDC_FILETYPE_ACTIONS 525
#define IDC_FILETYPE_NEW     526
#define IDC_FILETYPE_EDIT    527
#define IDC_FILETYPE_REMOVE  528
#define IDC_FILETYPE_DEFAULT 529
#define IDC_FILETYPE_SHOWEXT 530
#define IDC_FILEACTION_NAME  535     /* its actions */
#define IDC_FILEACTION_CMD   536
#define IDC_FILEACTION_BROWSE 537

/* Property pages: the controls of a page share an id range, which is how
   a tab shows its page. Separators (drawn etched) are hidden statics. */
#define IDC_PAGE1       1000    /* General: 1000..1049 */
/* The network's status: General */
#define IDC_NET_STATUS       1002
#define IDC_NET_DURATION     1004
#define IDC_NET_SPEED        1006
#define IDC_NET_PICTURE      1009
#define IDC_NET_SENT_PACKETS 1012
#define IDC_NET_IN_PACKETS   1013
#define IDC_NET_SENT_BYTES   1015
#define IDC_NET_IN_BYTES     1016
/* Support */
#define IDC_NET_TYPE         1052
#define IDC_NET_ADDRESS      1054
#define IDC_NET_MASK         1056
#define IDC_NET_GATEWAY      1058
#define IDC_NET_CARD         1061
#define IDC_NET_MAC          1063
#define IDC_NET_PROPERTIES   690
/* The network's settings: IP Address */
#define IDC_NP_AUTO          1000
#define IDC_NP_FIXED         1001
#define IDC_NP_IP            1003
#define IDC_NP_MASK          1005
#define IDC_NP_GATEWAY       1007
/* DNS */
#define IDC_NP_HOST          1051
#define IDC_NP_DOMAIN        1053
#define IDC_NP_DNS1          1055
#define IDC_NP_DNS2          1057
/* Card */
#define IDC_NP_CARD          1101
#define IDC_NP_IRQ           1103
#define IDC_NP_IO            1105
#define IDC_PROPS_ICON      1000
#define IDC_PROPS_NAME      1001
#define IDC_PROPS_TYPE      1002
#define IDC_PROPS_LOCATION  1003
#define IDC_PROPS_SIZE      1004
#define IDC_PROPS_CONTAINSL 1005
#define IDC_PROPS_CONTAINS  1006
#define IDC_PROPS_DATESL    1007    /* the labels of the four lines below */
#define IDC_PROPS_DOSNAME   1008
#define IDC_PROPS_CREATED   1009
#define IDC_PROPS_MODIFIED  1010
#define IDC_PROPS_ACCESSED  1011
#define IDC_PROPS_READONLY  1012
#define IDC_PROPS_HIDDEN    1013
#define IDC_PROPS_ARCHIVE   1014
#define IDC_PROPS_SYSTEM    1015
#define IDC_DRIVE_LABEL     1020    /* drives */
#define IDC_DRIVE_FS        1021
#define IDC_DRIVE_USEDBOX   1022
#define IDC_DRIVE_USED      1023
#define IDC_DRIVE_USEDS     1024
#define IDC_DRIVE_FREEBOX   1025
#define IDC_DRIVE_FREE      1026
#define IDC_DRIVE_FREES     1027
#define IDC_DRIVE_CAP       1028
#define IDC_DRIVE_CAPS      1029
#define IDC_DRIVE_PIE       1030
#define IDC_DRIVE_DRIVE     1031
#define IDC_DRIVE_CLEANUP   1032
#define IDC_COMPUTER_SYSTEM    1035    /* My Computer */
#define IDC_COMPUTER_VERSION   1036
#define IDC_COMPUTER_SHELL     1037
#define IDC_COMPUTER_CPU       1038
#define IDC_COMPUTER_MODE      1039
#define IDC_COMPUTER_MEMORY    1040
#define IDC_COMPUTER_RESOURCES 1041
#define IDC_PROPS_LABEL     1048
#define IDC_PROPS_SEP       1049
#define IDC_SHORTCUT_ICON      1050    /* Shortcut: 1050..1099 */
#define IDC_SHORTCUT_NAME      1051
#define IDC_SHORTCUT_TYPE      1052
#define IDC_SHORTCUT_LOCATION  1053
#define IDC_SHORTCUT_TARGET    1054
#define IDC_SHORTCUT_STARTIN   1055
#define IDC_SHORTCUT_RUN       1056
#define IDC_SHORTCUT_FIND      1057
#define IDC_SHORTCUT_ICONBTN   1058
#define IDC_SHORTCUT_KEY       1059
#define IDC_VERSION_FILEL      1100    /* Version: 1100..1149 */
#define IDC_VERSION_FILE       1101
#define IDC_VERSION_DESCL      1102
#define IDC_VERSION_DESC       1103
#define IDC_VERSION_COPYL      1104
#define IDC_VERSION_COPY       1105
#define IDC_VERSION_GROUP      1106
#define IDC_VERSION_ITEMSL     1107
#define IDC_VERSION_ITEMS      1108
#define IDC_VERSION_VALUEL     1109
#define IDC_VERSION_VALUE      1110
#define IDC_CUSTOM_ICONGROUP   1150    /* Customize (a folder's desktop.ini): 1150..1199 */
#define IDC_CUSTOM_ICONTEXT    1151
#define IDC_CUSTOM_ICON        1152
#define IDC_CUSTOM_CHANGE      1153
#define IDC_CUSTOM_DEFAULT     1154
#define IDC_CUSTOM_TIPGROUP    1155
#define IDC_CUSTOM_TIPL        1156
#define IDC_CUSTOM_TIP         1157
#define IDC_SHORTCUT_LABEL     1098
#define IDC_SHORTCUT_SEP       1099

/* Taskbar Properties' pages */
#define IDC_TASKBAR_PREVIEW   1000    /* Taskbar Options (hidden: its place is painted) */
#define IDC_TASKBAR_ONTOP     1001
#define IDC_TASKBAR_AUTOHIDE  1002
#define IDC_TASKBAR_SMALL     1003
#define IDC_TASKBAR_CLOCK     1004
#define IDC_TASKBAR_24HOUR    1005    /* WIN.INI [intl] iTime, as Control Panel's International */
#define IDC_ABOUT_BANNER      190     /* About: its painted banner, the link in blue */
#define IDC_ABOUT_LINK        191
#define IDC_TASKBAR_ICONSONLY 1006    /* [Taskbar] IconsOnly */
#define IDC_TASKBAR_GROUPED   1007    /* [Taskbar] Group */
#define IDC_TASKBAR_COLUMNS   1008    /* [Taskbar] TwoColumns: the Start menu's */
#define IDC_TASKBAR_CUSTOMIZE 1009    /* its size: IDD_STARTSIZE */
#define IDC_TASKBAR_GROUP     1050    /* Start Menu Programs */
#define IDC_TASKBAR_ICON      1051
#define IDC_TASKBAR_TEXT      1052
#define IDC_TASKBAR_ADD       1053
#define IDC_TASKBAR_REMOVE    1054
#define IDC_TASKBAR_ADVANCED  1055
#define IDC_TASKBAR_DOCGROUP  1056
#define IDC_TASKBAR_DOCTEXT   1057
#define IDC_TASKBAR_CLEAR     1058
#define IDC_TASKBAR_SYNCTEXT  1059
#define IDC_TASKBAR_SYNC      1060    /* Program Manager's groups */
#define IDC_BIN_NOMOVE        1000    /* Recycle Bin Properties: Global */
#define IDC_BIN_SLIDER        1001
#define IDC_BIN_PERCENT       1002
#define IDC_BIN_SIZELABEL     1003
#define IDC_BIN_CONFIRM       1004
#define IDC_TRAY_GROUP        1100    /* Tray: programs by the clock */
#define IDC_TRAY_TEXT         1101
#define IDC_TRAY_LIST         1102
#define IDC_TRAY_ADD          1103
#define IDC_TRAY_REMOVE       1104
#define IDC_HIDE_GROUP        1105    /* ... and programs kept off the taskbar */
#define IDC_HIDE_TEXT         1106
#define IDC_HIDE_LIST         1107
#define IDC_HIDE_ADD          1108
#define IDC_HIDE_REMOVE       1109

/* Folder Options' pages */
#define IDC_FOLDEROPTIONS_BROWSE    1000    /* General: Browse folders as follows */
#define IDC_FOLDEROPTIONS_SAME      1001
#define IDC_FOLDEROPTIONS_OWN       1002
#define IDC_FOLDEROPTIONS_CLICK     1003    /* Click items as follows */
#define IDC_FOLDEROPTIONS_SINGLE    1004
#define IDC_FOLDEROPTIONS_DOUBLE    1005
#define IDC_FOLDEROPTIONS_LABEL     1050    /* View */
#define IDC_FOLDEROPTIONS_LIST      1051
#define IDC_FOLDEROPTIONS_DEFAULTS  1052
#define IDC_FOLDEROPTIONS_VIEWS     1053    /* Folder views: Like Current Folder, Reset All Folders */
#define IDC_FOLDEROPTIONS_VIEWICON  1054
#define IDC_FOLDEROPTIONS_VIEWTEXT  1055
#define IDC_FOLDEROPTIONS_LIKE      1056
#define IDC_FOLDEROPTIONS_RESET     1057
#define IDC_FILETYPES_LABEL         1100    /* File Types */
#define IDC_FILETYPES_LIST          1101
#define IDC_FILETYPES_NEW           1102
#define IDC_FILETYPES_REMOVE        1103
#define IDC_FILETYPES_EDIT          1104
#define IDC_FILETYPES_DETAILS       1105
#define IDC_FILETYPES_ICON          1106
#define IDC_FILETYPES_EXTL          1107
#define IDC_FILETYPES_EXT           1108
#define IDC_FILETYPES_OPENSL        1109
#define IDC_FILETYPES_OPENS         1110

/* Display Properties' pages (the monitor and the Appearance picture are painted) */
#define IDC_BG_LISTL          1000    /* Background */
#define IDC_BG_LIST           1001
#define IDC_BG_BROWSE         1002
#define IDC_BG_STYLEL         1003
#define IDC_BG_STYLE          1004
#define IDC_SS_GROUP          1050    /* Screen Saver */
#define IDC_SS_LIST           1051
#define IDC_SS_SETTINGS       1052
#define IDC_SS_PREVIEW        1053
#define IDC_SS_WAITL          1054
#define IDC_SS_WAIT           1055
#define IDC_SS_MINUTES        1056
#define IDC_AP_SCHEMEL        1100    /* Appearance */
#define IDC_AP_SCHEME         1101
#define IDC_AP_SAVE           1102
#define IDC_AP_DELETE         1103
#define IDC_AP_ITEML          1104
#define IDC_AP_ITEM           1105
#define IDC_AP_COLORL         1106
#define IDC_AP_COLOR          1107
#define IDC_AP_TEXTL          1108
#define IDC_AP_TEXT           1109
#define IDC_AP_COLOR2L        1110
#define IDC_AP_COLOR2         1111
#define IDC_AP_THEMES         1112    /* opens the Theme Manager */
#define IDC_TM_PICTURE        1000    /* Theme Manager: Preview (hidden: painted) */
#define IDC_TM_SUMMARY        1001
#define IDC_TM_HASCOLORS      1050    /* Colors */
#define IDC_TM_COLORLIST      1051
#define IDC_TM_CHANGE         1052
#define IDC_TM_COLORHINT      1053
#define IDC_TM_HASWALL        1100    /* Desktop */
#define IDC_TM_WALLNAME       1101
#define IDC_TM_WALLBROWSE     1102
#define IDC_TM_WALLNONE       1103
#define IDC_TM_WALLSTYLE      1104
#define IDC_TM_HASSAVER       1105
#define IDC_TM_SAVER          1106
#define IDC_TM_HASICONS       1107
#define IDC_TM_ICONLIST       1108
#define IDC_TM_ICONCHANGE     1109
#define IDC_TM_ICONDEFAULT    1110
#define IDC_TM_HASSOUNDS      1150    /* Sounds */
#define IDC_TM_SOUNDLIST      1151
#define IDC_TM_SOUNDBROWSE    1152
#define IDC_TM_SOUNDPLAY      1153
#define IDC_TM_SOUNDNONE      1154
#define IDC_TM_HASPOINTERS    1200    /* Pointers */
#define IDC_TM_POINTERLIST    1201
#define IDC_TM_POINTERBROWSE  1202
#define IDC_TM_POINTERDEFAULT 1203
#define SLS_VERTICAL          0x0001  /* SbSlider up and down, its top the last position */
#define IDC_VOLUME_SLIDER     600
#define IDC_VOLUME_MUTE       601
#define IDC_RESMETER_BAR      610     /* + 0..2: System, User, GDI */
#define IDC_RESMETER_FREE     615     /* + 0..2 */
#define IDC_FORMAT_CAPACITY   620
#define IDC_FORMAT_QUICK      621
#define IDC_FORMAT_FULL       622
#define IDC_FORMAT_SYSONLY    623
#define IDC_FORMAT_LABEL      624
#define IDC_FORMAT_NOLABEL    625
#define IDC_FORMAT_SYS        626
#define IDC_CLEANUP_DRIVE     630
#define IDC_CLEANUP_ICON      631
#define IDC_CLEANUP_TEXT      632
#define IDC_CLEANUP_LIST      633
#define IDC_CLEANUP_TOTAL     634
#define IDC_CLEANUP_ABOUT     635
#define IDC_LOWDISK_ICON      636
#define IDC_LOWDISK_TEXT      637
#define IDC_DT_MONTH          640
#define IDC_DT_YEAR           641
#define IDC_DT_CALENDAR       642
#define IDC_DT_CLOCK          643
#define IDC_DT_TIME           644
#define IDC_EF_GROUP          1150    /* Effects */
#define IDC_EF_ANIMATE        1151
#define IDC_EF_SKIN           1152
#define IDC_EF_SHELL          1153
#define IDC_EF_RESGROUP       1154    /* USERX.DRV, GDIX.DRV */
#define IDC_EF_RESOURCES      1155
#define IDC_EF_RESNOTE        1156
#define IDC_ST_DISPLAYL       1200    /* Settings */
#define IDC_ST_DISPLAY        1201
#define IDC_ST_COLORSG        1202
#define IDC_ST_COLORS         1203
#define IDC_ST_AREAG          1204
#define IDC_ST_AREA           1205
#define IDC_ST_LESS           1206
#define IDC_ST_MORE           1207
#define IDC_ST_AREATEXT       1208

/* The Create Shortcut wizard's pages */
#define IDC_WIZARD_TEXT1     1000
#define IDC_WIZARD_CMDL      1001
#define IDC_WIZARD_CMD       1002
#define IDC_WIZARD_BROWSE    1003
#define IDC_WIZARD_TEXT2     1050
#define IDC_WIZARD_TREE      1051
#define IDC_WIZARD_NEWFOLDER 1052
#define IDC_WIZARD_TEXT3     1100
#define IDC_WIZARD_NAME      1101
#define IDC_WIZARD_TEXT4     1102

#endif
