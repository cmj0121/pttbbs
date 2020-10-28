#include "bbs.h"

// UNREGONLY 改為由 BASIC 來判斷是否為 guest.

#define CheckMenuPerm(x) \
    ( (x == MENU_UNREGONLY)? \
      ((!HasUserPerm(PERM_BASIC) || HasUserPerm(PERM_LOGINOK))?0:1) :\
	((!x) ? 1 :  \
         ((x & PERM_LOGINOK) ? HasBasicUserPerm(x) : HasUserPerm(x))))

/* help & menu processring */
static int      refscreen = NA;
extern char    *boardprefix;
extern struct utmpfile_t *utmpshm;

static const char *title_tail_msgs[] = {
    "看板",
    "系列",
    "文摘",
};
static const char *title_tail_attrs[] = {
    ANSI_COLOR(37),
    ANSI_COLOR(32),
    ANSI_COLOR(36),
};
enum {
    TITLE_TAIL_BOARD = 0,
    TITLE_TAIL_SELECT,
    TITLE_TAIL_DIGEST,
};

// 由於歷史因素，這裡會出現三種編號:
// MODE (定義於 modes.h)    是 BBS 對各種功能在 utmp 的編號 (var.c 要加字串)
// Menu Index (M_*)	    是 menu.c 內部分辨選單要對應哪個 mode 的 index
// AdBanner Index	    是動態看版要顯示什麼的值
// 從前這是用兩個 mode map 來轉換的 (令人看得滿頭霧水)
// 重整後 Menu Index 跟 AdBanner Index 合一，請見下面的說明
///////////////////////////////////////////////////////////////////////
// AdBanner (SHM->notes) 前幾筆是 Note 板精華區「<系統> 動態看板」(SYS)
// 目錄下的文章，所以編排 Menu (M_*) 時要照其順序：
// 精華區編號     => Menu Index => MODE
// (AdBannerIndex)
// ====================================
// 00離站畫面     =>  M_GOODBYE
// 01主選單       =>  M_MMENU   => MMENU
// 02系統維護區   =>  M_ADMIN   => ADMIN
// 03私人信件區   =>  M_MAIL    => MAIL
// 04休閒聊天區   =>  M_TMENU   => TMENU
// 05個人設定區   =>  M_UMENU   => UMENU
// 06系統工具區   =>  M_XMENU   => XMENU
// 07娛樂與休閒   =>  M_PMENU   => PMENU
// 08Ｐtt搜尋器   =>  M_SREG    => SREG
// 09Ｐtt量販店   =>  M_PSALE   => PSALE
// 10Ｐtt遊樂場   =>  M_AMUSE   => AMUSE
// 11Ｐtt棋院     =>  M_CHC     => CHC
// 12特別名單     =>  M_NMENU   => NMENU
///////////////////////////////////////////////////////////////////////
// 由於 MODE 與 menu 的順序現在已不一致 (最早可能是一致的)，所以中間的
// 轉換是靠 menu_mode_map 來處理。
// 要定義新 Menu 時，請在 M_MENU_MAX 之前加入新值，並在 menu_mode_map
// 加入對應的 MODE 值。 另外，在 Notes 下也要增加對應的 AdBanner 圖片
// 若不想加圖片則要修改 N_SYSADBANNER
///////////////////////////////////////////////////////////////////////

enum {
    M_GOODBYE=0,
    M_MMENU,	 M_ADMIN, M_MAIL, M_TMENU,
    M_UMENU,     M_XMENU, M_PMENU,M_SREG,
    M_PSALE,	 M_AMUSE, M_CHC,  M_NMENU,

    M_MENU_MAX,			// 這是 menu (M_*) 的最大值
    N_SYSADBANNER = M_MENU_MAX, // 定義 M_* 到多少有對應的 ADBANNER
    M_MENU_REFRESH= -1,		// 系統用不到的 index 值 (可顯示其它活動與點歌)
};

static const int menu_mode_map[M_MENU_MAX] = {
    0,
    MMENU,	ADMIN,	MAIL,	TMENU,
    UMENU,	XMENU,	PMENU,	SREG,
    PSALE,	AMUSE,	CHC,	NMENU
};

typedef struct {
    int     (*cmdfunc)();
    int     level;
    char    *desc;                   /* hotkey/description */
} commands_t;

///////////////////////////////////////////////////////////////////////

void
showtitle(const char *title, const char *mid)
{
    /* we have to...
     * - display title in left, cannot truncate.
     * - display mid message, cannot truncate
     * - display tail (board info), if possible.
     */
    int llen, rlen, mlen, mpos = 0;
    int pos = 0;
    int tail_type;
    const char *mid_attr = ANSI_COLOR(33);
    int is_currboard_special = 0;
    char buf[64];


    /* prepare mid */
#ifdef DEBUG
    {
	sprintf(buf, "  current pid: %6d  ", getpid());
	mid = buf;
	mid_attr = ANSI_COLOR(41;5);
    }
#else
    if (ISNEWMAIL(currutmp)) {
	mid = "    你有新信件    ";
	mid_attr = ANSI_COLOR(41;5);
    } else if ( HasUserPerm(PERM_ACCTREG) ) {
	// TODO cache this value?
	int nreg = regform_estimate_queuesize();
	if(nreg > 100)
	{
	    nreg -= (nreg % 10);
	    sprintf(buf, "  超過 %03d 篇未審核  ", nreg);
	    mid_attr = ANSI_COLOR(41;5);
	    mid = buf;
	}
    }
#endif

    /* prepare tail */
    if (currmode & MODE_SELECT)
	tail_type = TITLE_TAIL_SELECT;
    else if (currmode & MODE_DIGEST)
	tail_type = TITLE_TAIL_DIGEST;
    else
	tail_type = TITLE_TAIL_BOARD;

    if(currbid > 0)
    {
	assert(0<=currbid-1 && currbid-1<MAX_BOARD);
	is_currboard_special = (
		(getbcache(currbid)->brdattr & BRD_HIDE) &&
		(getbcache(currbid)->brdattr & BRD_POSTMASK));
    }

    /* now, calculate real positioning info */
    llen = strlen(title);
    mlen = strlen(mid);
    mpos = (t_columns -1 - mlen)/2;

    /* first, print left. */
    clear();
    outs(TITLE_COLOR "【");
    outs(title);
    outs("】");
    pos = llen + 4;

    /* print mid */
    while(pos++ < mpos)
	outc(' ');
    outs(mid_attr);
    outs(mid);
    pos += mlen;
    outs(TITLE_COLOR);

    /* try to locate right */
    rlen = strlen(currboard) + 4 + 4;
    if(currboard[0] && pos+rlen < t_columns)
    {
	// print right stuff
	while(pos++ < t_columns-rlen)
	    outc(' ');
	outs(title_tail_attrs[tail_type]);
	outs(title_tail_msgs[tail_type]);
	outs("《");

	if (is_currboard_special)
	    outs(ANSI_COLOR(32));
	outs(currboard);
	outs(title_tail_attrs[tail_type]);
	outs("》" ANSI_RESET "\n");
    } else {
	// just pad it.
	while(pos++ < t_columns)
	    outc(' ');
	outs(ANSI_RESET "\n");
    }

}

int TopBoards(void);

/* Ctrl-Z Anywhere Fast Switch, not ZG. */
static char zacmd = 0;

// ZA is waiting, hurry to the meeting stone!
int
ZA_Waiting(void)
{
    return (zacmd != 0);
}

void
ZA_Drop(void)
{
    zacmd = 0;
}

// Promp user our ZA bar and return for selection.
int
ZA_Select(void)
{
    int k;

    if (!is_login_ready ||
        !HasUserPerm(PERM_BASIC) ||
        HasUserPerm(PERM_VIOLATELAW))
        return 0;

    // TODO refresh status bar?
    vs_footer(VCLR_ZA_CAPTION " ★快速切換: ",
	    " (b)文章列表 (c)分類 (t)熱門 (f)我的最愛 (m)信箱 (u)使用者名單");
    k = vkey();

    if (k < ' ' || k >= 'z') return 0;
    k = tolower(k);

    if(strchr("bcfmut", k) == NULL)
	return 0;

    zacmd = k;
    return 1;
}

// The ZA processor, only invoked in menu.
void
ZA_Enter(void)
{
    char cmd = zacmd;
    while (zacmd)
    {
	cmd = zacmd;
	zacmd = 0;

	// All ZA applets must check ZA_Waiting() at every stack of event loop.
	switch(cmd) {
	    case 'b':
		Read();
		break;
	    case 'c':
		Class();
		break;
	    case 't':
		TopBoards();
		break;
	    case 'f':
		Favorite();
		break;
	    case 'm':
		m_read();
		break;
	    case 'u':
		t_users();
		break;
	}
	// if user exit with new ZA assignment,
	// direct enter in next loop.
    }
}

/* 動畫處理 */
#define FILMROW 11
static unsigned short menu_row = 12;
static unsigned short menu_column = 20;

#ifdef EXP_ALERT_ADBANNER_USONG
static int
decide_menu_row(const commands_t *p) {
    if ((p[0].level && !HasUserPerm(p[0].level)) &&
        HasUserFlag(UF_ADBANNER_USONG) &&
        HasUserFlag(UF_ADBANNER)) {
        return menu_row + 1;
    }

    return menu_row;
}
#else
# define decide_menu_row(x) (menu_row)
#endif

static void
show_status(void)
{
    int i;
    struct tm      ptime;
    char           *myweek = "天一二三四五六";

    localtime4_r(&now, &ptime);
    i = ptime.tm_wday << 1;
    move(b_lines, 0);
    vbarf(ANSI_COLOR(34;46) "[%d/%d 星期%c%c %d:%02d]"
	  ANSI_COLOR(1;33;45) "%-14s"
	  ANSI_COLOR(30;47) " 線上" ANSI_COLOR(31)
	  "%d" ANSI_COLOR(30) "人, 我是" ANSI_COLOR(31) "%s"
	  ANSI_COLOR(30) "\t[呼叫器]" ANSI_COLOR(31) "%s ",
	  ptime.tm_mon + 1, ptime.tm_mday, myweek[i], myweek[i + 1],
	  ptime.tm_hour, ptime.tm_min, SHM->today_is,
	  SHM->UTMPnumber, cuser.userid,
	  str_pager_modes[currutmp->pager % PAGER_MODES]);
}

/*
 * current caller of adbanner:
 *   xyz.c:   adbanner_goodbye();   // logout
 *   menu.c:  adbanner(cmdmode);    // ...
 *   board.c: adbanner(0);	    // 後來變在 board.c 裡自己處理(應該是那隻魚)
 */

void
adbanner_goodbye()
{
    adbanner(M_GOODBYE);
}

void
adbanner(int menu_index)
{
    int i = menu_index;

    // don't show if stat in class or user wants to skip adbanners
    if (currstat == CLASS || !(HasUserFlag(UF_ADBANNER)))
	return;

    // also prevent SHM busy status
    if (SHM->Pbusystate || SHM->last_film <= 0)
	return;

    if (    i != M_MENU_REFRESH &&
	    i >= 0		&&
	    i <  N_SYSADBANNER  &&
	    i <= SHM->last_film)
    {
	// use system menu - i
    } else {
	// To display ADBANNERs in slide show mode.
	// Since menu is updated per hour, the total presentation time
	// should be less than one hour. 60*60/MAX_ADBANNER[500]=7 (seconds).
	// @ Note: 60 * 60 / MAX_ADBANNER =3600/MAX_ADBANNER = "how many seconds
	// can one ADBANNER to display" to slide through every banners in one hour.
	// @ now / (3600 / MAx_ADBANNER) means "get the index of which to show".
	// syncnow();

	const int slideshow_duration = 3600 / MAX_ADBANNER,
		  slideshow_index    = now  / slideshow_duration;

	// index range: 0 =>[system] => N_SYSADBANNER    => [user esong] =>
	//              last_usong   => [advertisements] => last_film
	int valid_usong_range = (SHM->last_usong > N_SYSADBANNER &&
				 SHM->last_usong < SHM->last_film);

	if (SHM->last_film > N_SYSADBANNER) {
	    if (HasUserFlag(UF_ADBANNER_USONG) || !valid_usong_range)
		i = N_SYSADBANNER +       slideshow_index % (SHM->last_film+1-N_SYSADBANNER);
	    else
		i = SHM->last_usong + 1 + slideshow_index % (SHM->last_film - SHM->last_usong);
	}
	else
	    i = 0; // SHM->last_film;
    }

    // make it safe!
    i %= MAX_ADBANNER;

    move(1, 0);
    clrtoln(1 + FILMROW);	/* 清掉上次的 */
#ifdef LARGETERM_CENTER_MENU
    out_lines(SHM->notes[i], 11, (t_columns - 80)/2);	/* 只印11行就好 */
#else
    out_lines(SHM->notes[i], 11, 0);	/* 只印11行就好 */
#endif
    outs(ANSI_RESET);
#ifdef DEBUG
    // XXX piaip test
    move(FILMROW, 0); prints(" [ %d ] ", i);
#endif
}

static int
show_menu(int menu_index, const commands_t * p)
{
    register int    n = 0;
    register char  *s;
    int row = menu_row;

    adbanner(menu_index);

    // seems not everyone likes the menu in center.
#ifdef LARGETERM_CENTER_MENU
    // update menu column [fixed const because most items are designed as fixed)
    menu_column = (t_columns-40)/2;
    row = 12 + (t_lines-24)/2;
#endif

#ifdef EXP_ALERT_ADBANNER_USONG
    if ((p[0].level && !HasUserPerm(p[0].level)) &&
        HasUserFlag(UF_ADBANNER_USONG) &&
        HasUserFlag(UF_ADBANNER)) {
        // we have one more extra line to display ADBANNER_USONG!
        int alert_column = menu_column;
        move(row, 0);
        vpad(t_columns-2, "─");
        if (alert_column > 2)
            alert_column -= 2;
        alert_column -= alert_column % 2;
        move(row++, alert_column);
        outs(" 上方為使用者心情點播留言區，不代表本站立場 ");
    }
    assert(row == decide_menu_row(p));
#endif

    move(row, 0);
    while ((s = p[n].desc)) {
	if (CheckMenuPerm(p[n].level)) {
            prints("%*s  (%s%c" ANSI_RESET ")%s\n",
                   menu_column, "",
                   (HasUserFlag(UF_MENU_LIGHTBAR) ? ANSI_COLOR(36) :
                    ANSI_COLOR(1;36)), s[0], s+1);
	}
	n++;
    }
    return n - 1;
}

static void
domenu(int menu_index, const char *cmdtitle, int cmd, const commands_t cmdtable[])
{
    int             lastcmdptr, cmdmode;
    int             n, pos, total, i;
    int             err;

    assert(0 <= menu_index && menu_index < M_MENU_MAX);
    cmdmode = menu_mode_map[menu_index];

    setutmpmode(cmdmode);
    showtitle(cmdtitle, BBSName);
    total = show_menu(menu_index, cmdtable);

    show_status();
    lastcmdptr = pos = 0;

    do {
	i = -1;
	switch (cmd) {
	case Ctrl('Z'):
	    ZA_Select(); // we'll have za loop later.
	    refscreen = YEA;
	    i = lastcmdptr;
	    break;
	case Ctrl('N'):
	    New();
	    refscreen = YEA;
	    i = lastcmdptr;
	    break;
	case Ctrl('A'):
	    if (mail_man() == FULLUPDATE)
		refscreen = YEA;
	    i = lastcmdptr;
	    break;
	case KEY_DOWN:
	    i = lastcmdptr;
	case KEY_HOME:
	case KEY_PGUP:
	    do {
		if (++i > total)
		    i = 0;
	    } while (!CheckMenuPerm(cmdtable[i].level));
	    break;
	case KEY_UP:
	    i = lastcmdptr;
	case KEY_END:
	case KEY_PGDN:
	    do {
		if (--i < 0)
		    i = total;
	    } while (!CheckMenuPerm(cmdtable[i].level));
	    break;
	case KEY_LEFT:
	case 'e':
	case 'E':
	    if (cmdmode == MMENU)
		cmd = 'G';	    // to exit
	    else if ((cmdmode == MAIL) && chkmailbox())
		cmd = 'R';	    // force keep reading mail
	    else
		return;
	default:
	    if ((cmd == 's' || cmd == 'r') &&
		(cmdmode == MMENU || cmdmode == TMENU || cmdmode == XMENU)) {
		if (cmd == 's')
		    ReadSelect();
		else
		    Read();
		refscreen = YEA;
		i = lastcmdptr;
		currstat = cmdmode;
		break;
	    }
	    if (cmd == KEY_ENTER || cmd == KEY_RIGHT) {
		move(b_lines, 0);
		clrtoeol();

		currstat = XMODE;

		if ((err = (*cmdtable[lastcmdptr].cmdfunc) ()) == QUIT)
		    return;
		currutmp->mode = currstat = cmdmode;

		if (err == XEASY) {
		    refresh();
		    safe_sleep(1);
		} else if (err != XEASY + 1 || err == FULLUPDATE)
		    refscreen = YEA;

                // keep current position
		i = lastcmdptr;
                break;
	    }

	    if (cmd >= 'a' && cmd <= 'z')
		cmd = toupper(cmd);
	    while (++i <= total && cmdtable[i].desc)
		if (cmdtable[i].desc[0] == cmd)
		    break;

	    if (!CheckMenuPerm(cmdtable[i].level)) {
		for (i = 0; cmdtable[i].cmdfunc; i++)
		    if (CheckMenuPerm(cmdtable[i].level))
			break;
		if (!cmdtable[i].cmdfunc)
		    return;
	    }

	    if (cmd == 'H' && i > total){
		/* TODO: Add menu help */
	    }
	}

	// end of all commands
	if (ZA_Waiting())
	{
	    ZA_Enter();
	    refscreen = 1;
	    currstat = cmdmode;
	}

	if (i > total || !CheckMenuPerm(cmdtable[i].level))
	    continue;

	if (refscreen) {
	    showtitle(cmdtitle, BBSName);
	    // menu 設定 M_MENU_REFRESH 可讓 ADBanner 顯示別的資訊
	    show_menu(M_MENU_REFRESH, cmdtable);
	    show_status();
	    refscreen = NA;
	}
	cursor_clear(decide_menu_row(cmdtable) + pos, menu_column);
	n = pos = -1;
	while (++n <= (lastcmdptr = i))
	    if (CheckMenuPerm(cmdtable[n].level))
		pos++;

        // If we want to replace cursor_show by cursor_key, it must be inside
        // while(expr) othrewise calling 'continue' inside for-loop won't wait
        // for key.
	cursor_show(decide_menu_row(cmdtable) + pos, menu_column);
    } while (((cmd = vkey()) != EOF) || refscreen);

    abort_bbs(0);
}
/* INDENT OFF */

static int
view_user_money_log() {
    char userid[IDLEN+1];
    char fpath[PATHLEN];

    vs_hdr("檢視使用者交易記錄");
    usercomplete("請輸入要檢視的ID: ", userid);
    if (!is_validuserid(userid))
        return 0;
    sethomefile(fpath, userid, FN_RECENTPAY);
    if (more(fpath, YEA) < 0)
        vmsgf("使用者 %s 無最近交易記錄", userid);
    return 0;
}

static int
view_user_login_log() {
    char userid[IDLEN+1];
    char fpath[PATHLEN];

    vs_hdr("檢視使用者最近上線記錄");
    usercomplete("請輸入要檢視的ID: ", userid);
    if (!is_validuserid(userid))
        return 0;
    sethomefile(fpath, userid, FN_RECENTLOGIN);
    if (more(fpath, YEA) < 0)
        vmsgf("使用者 %s 無最近上線記錄", userid);
    return 0;
}

static int x_admin_money(void);
static int x_admin_user(void);

static int deprecate_userlist() {
    vs_hdr2(" " BBSNAME " ", " 已移至使用者名單");
    outs("\n"
         "此功能已移至使用者名單區。\n"
         "請至使用者名單 (Ctrl-U) 並按下對應的鍵。\n"
         "(在使用者名單按 h 會有完整說明)\n\n"
         "切換呼叫器:     Ctrl-U p\n"
         "隱身術:         Ctrl-U C\n"
         "顯示上幾次熱訊: Ctrl-U l\n");
    pressanykey();
    return 0;
}

// ----------------------------------------------------------- MENU DEFINITION
// 注意每個 menu 最多不能同時顯示超過 11 項 (80x24 標準大小的限制)

static const commands_t m_admin_money[] = {
    {view_user_money_log, PERM_SYSOP|PERM_ACCOUNTS,
                                                "View Log      檢視交易記錄"},
    {give_money, PERM_SYSOP|PERM_VIEWSYSOP,	"Givemoney     紅包雞"},
    {NULL, 0, NULL}
};

static const commands_t m_admin_user[] = {
    {view_user_money_log, PERM_SYSOP|PERM_ACCOUNTS,
                                        "Money Log      最近交易記錄"},
    {view_user_login_log, PERM_SYSOP|PERM_ACCOUNTS|PERM_BOARD,
                                        "OLogin Log     最近上線記錄"},
    {u_list, PERM_SYSOP,		"Users List     列出註冊名單"},
    {search_user_bybakpwd, PERM_SYSOP|PERM_ACCOUNTS,
                                        "DOld User data 查閱備份使用者資料"},
    {NULL, 0, NULL}
};

/* administrator's maintain menu */
static const commands_t adminlist[] = {
    {m_user, PERM_SYSOP,		"User          �ϥΪ̸��"},
    {m_board, PERM_BOARD,		"Board         �]�w�ݪO"},
    {m_register,
	PERM_ACCOUNTS|PERM_ACCTREG,	"Register      審核註冊表單"},
    {x_file, PERM_SYSOP|PERM_VIEWSYSOP,	"Xfile         編輯系統檔案"},
    {x_admin_money, PERM_SYSOP|PERM_ACCOUNTS|PERM_VIEWSYSOP,
                                        "Money         【" MONEYNAME "相關】"},
    {x_admin_user, PERM_SYSOP|PERM_ACCOUNTS|PERM_BOARD|PERM_POLICE_MAN,
                                        "LUser Log     【使用者資料記錄】"},
    {search_user_bypwd,
	PERM_ACCOUNTS|PERM_POLICE_MAN,	"Search User    �S���j�M�ϥΪ�"},
#ifdef USE_VERIFYDB
    {verifydb_admin_search_display,
	PERM_ACCOUNTS,			"Verify Search  �j�M�{�Ҹ�Ʈw"},
#endif
    {m_loginmsg, PERM_SYSOP,		"GMessage Login �i�����y"},
    {NULL, 0, NULL}
};

/* mail menu */
static const commands_t maillist[] = {
    {m_read, PERM_READMAIL,     "Read          我的信箱"},
    {m_send, PERM_LOGINOK,      "Send          站內寄信"},
    {mail_list, PERM_LOGINOK,   "Mail List     群組寄信"},
    {setforward, PERM_LOGINOK,  "Forward       設定信箱自動轉寄" },
    {mail_mbox, PERM_INTERNET,  "Zip UserHome  把所有私人資料打包回去"},
    {built_mail_index,
	PERM_LOGINOK,		"Savemail      重建信箱索引"},
    {mail_all, PERM_SYSOP,      "All           寄信給所有使用者"},
#ifdef USE_MAIL_ACCOUNT_SYSOP
    {mail_account_sysop, 0,     "Contact AM    寄信給帳號站長"},
#endif
    {NULL, 0, NULL}
};

#ifdef PLAY_ANGEL
static const commands_t angelmenu[] = {
    {a_angelmsg, PERM_ANGEL,"Leave message 留言給小主人"},
    {a_angelmsg2,PERM_ANGEL,"Call screen   呼叫畫面個性留言"},
    {angel_check_master,PERM_ANGEL,
                            "Master check  查詢小主人狀態"},
    // Cannot use R because r is reserved for Read/Mail due to TMENU.
    {a_angelreport, 0,      "PReport       線上天使狀態報告"},
    {NULL, 0, NULL}
};

static int menu_angelbeats() {
    domenu(M_TMENU, "Angel Beats! 天使公會", 'L', angelmenu);
    return 0;
}
#endif

/* Talk menu */
static const commands_t talklist[] = {
    {t_users, 0,            "Users         線上使用者列表"},
    {t_query, 0,            "Query         查詢網友"},
    // PERM_PAGE - 水球都要 PERM_LOGIN 了
    // 沒道理可以 talk 不能水球。
    {t_talk, PERM_LOGINOK,  "Talk          找人聊聊"},
    // PERM_CHAT 非 login 也有，會有人用此吵別人。
    {t_chat, PERM_LOGINOK,  "Chat          【" BBSMNAME2 "多人聊天室】"},
    {deprecate_userlist, 0, "Pager         切換呼叫器"},
    {t_qchicken, 0,         "Watch Pet     查詢寵物"},
#ifdef PLAY_ANGEL
    {a_changeangel,
	PERM_LOGINOK,	    "AChange Angel 更換小天使"},
    {menu_angelbeats, PERM_ANGEL|PERM_SYSOP,
                            "BAngel Beats! 天使公會"},
#endif
    {t_display, 0,          "Display       顯示上幾次熱訊"},
    {NULL, 0, NULL}
};

/* name menu */
static int t_aloha() {
    friend_edit(FRIEND_ALOHA);
    return 0;
}

static int t_special() {
    friend_edit(FRIEND_SPECIAL);
    return 0;
}

static const commands_t namelist[] = {
    {t_override, PERM_LOGINOK,"OverRide      好友名單"},
    {t_reject, PERM_LOGINOK,  "Black         壞人名單"},
    {t_aloha,PERM_LOGINOK,    "ALOHA         上站通知名單"},
    {t_fix_aloha,PERM_LOGINOK,"XFixALOHA     修正上站通知"},
    {t_special,PERM_LOGINOK,  "Special       其他特別名單"},
    {NULL, 0, NULL}
};

static int u_view_recentlogin()
{
    char fn[PATHLEN];
    setuserfile(fn, FN_RECENTLOGIN);
    return more(fn, YEA);
}

#ifdef USE_RECENTPAY
static int u_view_recentpay()
{
    char fn[PATHLEN];
    clear();
    mvouts(10, 5, "注意: 此處內容僅供參考，實際" MONEYNAME
                        "異動以站方內部資料為準");
    pressanykey();
    setuserfile(fn, FN_RECENTPAY);
    return more(fn, YEA);
}
#endif

static const commands_t myfilelist[] = {
    {u_editplan,    PERM_LOGINOK,   "QueryEdit     編輯名片檔"},
    {u_editsig,	    PERM_LOGINOK,   "Signature     編輯簽名檔"},
    {NULL, 0, NULL}
};

static const commands_t myuserlog[] = {
    {u_view_recentlogin, 0,   "LRecent Login  最近上站記錄"},
#ifdef USE_RECENTPAY
    {u_view_recentpay,   0,   "PRecent Pay    最近交易記錄"},
#endif
    {NULL, 0, NULL}
};

static int
u_myfiles()
{
    domenu(M_UMENU, "個人檔案", 'Q', myfilelist);
    return 0;
}

static int
u_mylogs()
{
    domenu(M_UMENU, "個人記錄", 'L', myuserlog);
    return 0;
}

void Customize(); // user.c

static int
u_customize()
{
    Customize();
    return 0;
}


/* User menu */
static const commands_t userlist[] = {
    {u_customize,   PERM_BASIC,	    "UCustomize    個人化設定"},
    {u_info,	    PERM_BASIC,     "Info          設定個人資料與密碼"},
    {u_loginview,   PERM_BASIC,     "VLogin View   選擇進站畫面"},
    {u_myfiles,	    PERM_LOGINOK,   "My Files      【個人檔案】 (名片,簽名檔...)"},
    {u_mylogs,	    PERM_LOGINOK,   "LMy Logs      【個人記錄】 (最近上線...)"},
    {u_register,    MENU_UNREGONLY, "Register      填寫《註冊申請單》"},
#ifdef ASSESS
    {u_cancelbadpost,PERM_LOGINOK,  "Bye BadPost   申請刪除退文"},
#endif // ASSESS
    {deprecate_userlist,       0,   "KCloak        隱身術"},
    {NULL, 0, NULL}
};

#ifdef HAVE_USERAGREEMENT
static int
x_agreement(void)
{
    more(HAVE_USERAGREEMENT, YEA);
    return 0;
}
#endif

static int
x_admin_money(void)
{
    char init = 'V';
    if (HasUserPerm(PERM_VIEWSYSOP))
        init = 'G';
    domenu(M_XMENU, "金錢相關管理", init, m_admin_money);
    return 0;
}

static int
x_admin_user(void)
{
    domenu(M_XMENU, "使用者記錄管理", 'O', m_admin_user);
    return 0;
}

#ifdef HAVE_INFO
static int
x_program(void)
{
    more("etc/version", YEA);
    return 0;
}
#endif

#ifdef HAVE_LICENSE
static int
x_gpl(void)
{
    more("etc/GPL", YEA);
    return 0;
}
#endif

#ifdef HAVE_SYSUPDATES
static int
x_sys_updates(void)
{
    more("etc/sysupdates", YEA);
    return 0;
}
#endif

#ifdef DEBUG
int _debug_reportstruct()
{
    clear();
    prints("boardheader_t:\t%d\n", sizeof(boardheader_t));
    prints("fileheader_t:\t%d\n", sizeof(fileheader_t));
    prints("userinfo_t:\t%d\n", sizeof(userinfo_t));
    prints("screenline_t:\t%d\n", sizeof(screenline_t));
    prints("SHM_t:\t%d\n", sizeof(SHM_t));
    prints("userec_t:\t%d\n", sizeof(userec_t));
    pressanykey();
    return 0;
}

#endif

/* XYZ tool sub menu */
static const commands_t m_xyz_hot[] = {
    {x_week, 0,      "Week          《本週五十大熱門話題》"},
    {x_issue, 0,     "Issue         《今日十大熱門話題》"},
    {x_boardman,0,   "Man Boards    《看板精華區排行榜》"},
    {NULL, 0, NULL}
};

/* XYZ tool sub menu */
static const commands_t m_xyz_user[] = {
    {x_user100 ,0,   "Users         《使用者百大排行榜》"},
    {topsong,PERM_LOGINOK,
	             "GTop Songs    《使用者心情點播排行》"},
    {x_today, 0,     "Today         《今日上線人次統計》"},
    {x_yesterday, 0, "Yesterday     《昨日上線人次統計》"},
    {NULL, 0, NULL}
};

static int
x_hot(void)
{
    domenu(M_XMENU, "熱門話題與看板", 'W', m_xyz_hot);
    return 0;
}

static int
x_users(void)
{
    domenu(M_XMENU, "使用者統計資訊", 'U', m_xyz_user);
    return 0;
}

/* XYZ tool menu */
static const commands_t xyzlist[] = {
    {x_hot,  0,      "THot Topics   《熱門話題與看板》"},
    {x_users,0,      "Users         《使用者相關統計》"},
#ifndef DEBUG
    /* All these are useless in debug mode. */
#ifdef HAVE_USERAGREEMENT
    {x_agreement,0,  "Agreement     《本站使用者條款》"},
#endif
#ifdef  HAVE_LICENSE
    {x_gpl, 0,       "ILicense       GNU 使用執照"},
#endif
#ifdef HAVE_INFO
    {x_program, 0,   "Program       本程式之版本與版權宣告"},
#endif
    {x_history, 0,   "History       《我們的成長》"},
    {x_login,0,      "System        《系統重要公告》"},
#ifdef HAVE_SYSUPDATES
    {x_sys_updates,0,"LUpdates      《本站系統程式更新紀錄》"},
#endif

#else // !DEBUG
    {_debug_reportstruct, 0,
	    	     "ReportStruct  報告各種結構的大小"},
#endif // !DEBUG

    {p_sysinfo, 0,   "Xinfo         《查看系統資訊》"},
    {NULL, 0, NULL}
};

/* Ptt money menu */
static const commands_t moneylist[] = {
    {p_give, 0,         "0Give        給其他人" MONEYNAME},
    {save_violatelaw, 0,"1ViolateLaw  繳罰單"},
    {p_from, 0,         "2From        暫時修改故鄉"},
    {ordersong,0,       "3OSong       心情點播機"},
    {NULL, 0, NULL}
};

static const commands_t      cmdlist[] = {
    {admin, PERM_SYSOP|PERM_ACCOUNTS|PERM_BOARD|PERM_VIEWSYSOP|PERM_ACCTREG|PERM_POLICE_MAN,
				"0Admin       【 系統維護區 】"},
    {Announce,	0,		"Announce     【 精華公佈欄 】"},
#ifdef DEBUG
    {Favorite,	0,		"Favorite     【 我的最不愛 】"},
#else
    {Favorite,	0,		"Favorite     【 我 的 最愛 】"},
#endif
    {Class,	0,		"Class        【 分組討論區 】"},
    // TODO 目前很多人被停權時會變成 -R-1-3 (PERM_LOGINOK, PERM_VIOLATELAW,
    // PERM_NOREGCODE) 沒有 PERM_READMAIL，但這樣麻煩的是他們就搞不懂發生什麼事
    {Mail, 	PERM_BASIC,     "Mail         【 私人信件區 】"},
    // 有些 bot 喜歡整天 query online accounts, 所以聊天改為 LOGINOK
    {Talk, 	PERM_LOGINOK,	"Talk         【 休閒聊天區 】"},
    {User, 	PERM_BASIC,	"User         【 個人設定區 】"},
    {Xyz, 	0,		"Xyz          【 系統資訊區 】"},
    {Play_Play, PERM_LOGINOK, 	"Play         【 娛樂與休閒 】"},
    {Name_Menu, PERM_LOGINOK,	"Namelist     【 編特別名單 】"},
#ifdef DEBUG
    {Goodbye, 	0, 		"Goodbye      再見再見再見再見"},
#else
    {Goodbye, 	0, 		"Goodbye         離開，再見… "},
#endif
    {NULL, 	0, 		NULL}
};

int main_menu(void) {
    domenu(M_MMENU, "主功能表", (ISNEWMAIL(currutmp) ? 'M' : 'C'), cmdlist);
    return 0;
}

static int p_money() {
    domenu(M_PSALE, BBSMNAME2 "量販店", '0', moneylist);
    return 0;
};

static int chessroom();

/* Ptt Play menu */
static const commands_t playlist[] = {
    {p_money, PERM_LOGINOK,  "Pay         【 " BBSMNAME2 "量販店 】"},
    {chicken_main, PERM_LOGINOK,
			     "Chicken     【 " BBSMNAME2 "養雞場 】"},
    {ticket_main, PERM_LOGINOK,
                             "Gamble      【 " BBSMNAME2 "彩券   】"},
    {chessroom, PERM_LOGINOK,"BChess      【 " BBSMNAME2 "棋院   】"},
    {NULL, 0, NULL}
};

static const commands_t conn6list[] = {
    {conn6_main,       PERM_LOGINOK, "1Conn6Fight    【" ANSI_COLOR(1;33) "六子棋邀局" ANSI_RESET "】"},
    {conn6_personal,   PERM_LOGINOK, "2Conn6Self     【" ANSI_COLOR(1;34) "六子棋打譜" ANSI_RESET "】"},
    {conn6_watch,      PERM_LOGINOK, "3Conn6Watch    【" ANSI_COLOR(1;35) "六子棋觀棋" ANSI_RESET "】"},
    {NULL, 0, NULL}
};

static int conn6_menu() {
    domenu(M_CHC, BBSMNAME2 "六子棋", '1', conn6list);
    return 0;
}

static const commands_t chesslist[] = {
    {chc_main,         PERM_LOGINOK, "1CChessFight   【" ANSI_COLOR(1;33) " 象棋邀局 " ANSI_RESET "】"},
    {chc_personal,     PERM_LOGINOK, "2CChessSelf    【" ANSI_COLOR(1;34) " 象棋打譜 " ANSI_RESET "】"},
    {chc_watch,        PERM_LOGINOK, "3CChessWatch   【" ANSI_COLOR(1;35) " 象棋觀棋 " ANSI_RESET "】"},
    {gomoku_main,      PERM_LOGINOK, "4GomokuFight   【" ANSI_COLOR(1;33) "五子棋邀局" ANSI_RESET "】"},
    {gomoku_personal,  PERM_LOGINOK, "5GomokuSelf    【" ANSI_COLOR(1;34) "五子棋打譜" ANSI_RESET "】"},
    {gomoku_watch,     PERM_LOGINOK, "6GomokuWatch   【" ANSI_COLOR(1;35) "五子棋觀棋" ANSI_RESET "】"},
    {gochess_main,     PERM_LOGINOK, "7GoChessFight  【" ANSI_COLOR(1;33) " 圍棋邀局 " ANSI_RESET "】"},
    {gochess_personal, PERM_LOGINOK, "8GoChessSelf   【" ANSI_COLOR(1;34) " 圍棋打譜 " ANSI_RESET "】"},
    {gochess_watch,    PERM_LOGINOK, "9GoChessWatch  【" ANSI_COLOR(1;35) " 圍棋觀棋 " ANSI_RESET "】"},
    {conn6_menu,       PERM_LOGINOK, "CConnect6      【" ANSI_COLOR(1;33) "  六子棋  " ANSI_RESET "】"},
    {NULL, 0, NULL}
};

static int chessroom() {
    domenu(M_CHC, BBSMNAME2 "棋院", '1', chesslist);
    return 0;
}

// ---------------------------------------------------------------- SUB MENUS

/* main menu */

int
admin(void)
{
    char init = 'L';

    if (HasUserPerm(PERM_VIEWSYSOP))
        init = 'X';
    else if (HasUserPerm(PERM_ACCTREG))
        init = 'R';
    else if (HasUserPerm(PERM_POLICE_MAN))
        init = 'S';

    domenu(M_ADMIN, "系統維護", init, adminlist);
    return 0;
}

int
Mail(void)
{
    domenu(M_MAIL, "電子郵件", 'R', maillist);
    return 0;
}

int
Talk(void)
{
    domenu(M_TMENU, "聊天說話", 'U', talklist);
    return 0;
}

int
User(void)
{
    domenu(M_UMENU, "個人設定", 'U', userlist);
    return 0;
}

int
Xyz(void)
{
    domenu(M_XMENU, "工具程式", 'T', xyzlist);
    return 0;
}

int
Play_Play(void)
{
    domenu(M_PMENU, "網路遊樂場", 'G', playlist);
    return 0;
}

int
Name_Menu(void)
{
    domenu(M_NMENU, "名單編輯", 'O', namelist);
    return 0;
}

