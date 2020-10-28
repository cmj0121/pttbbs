#define PWCU_IMPL
#include "bbs.h"
#include "daemons.h"

#define FN_REGISTER_LOG  "register.log"	// global registration history
#define FN_REJECT_NOTIFY "justify.reject"
#define FN_NOTIN_WHITELIST_NOTICE "etc/whitemail.notice"

// Regform1 file name (deprecated)
#define fn_register	"register.new"

// New style (Regform2) file names:
#define FN_REGFORM	"regform"	// registration form in user home
#define FN_REGFORM_LOG	"regform.log"	// regform history in user home
#define FN_REQLIST	"reg.wait"	// request list file, in global directory (replacing fn_register)

#define FN_REJECT_NOTES	"etc/reg_reject.notes"
#define REGNOTES_ROOT "etc/regnotes/"	// a folder to hold detail description

#define FN_JOBSPOOL_DIR	"jobspool/"

#define FN_REJECT_STR_NAME "etc/reg_reject_str.name"
#define FN_REJECT_STR_ADDR "etc/reg_reject_str.addr"
#define FN_REJECT_STR_CAREER "etc/reg_reject_str.career"

// #define DBG_DISABLE_CHECK	// disable all input checks
// #define DBG_DRYRUN	// Dry-run test (mainly for RegForm2)

#define MSG_ERR_MAXTRIES "您嘗試錯誤的輸入次數太多，請下次再來吧"

////////////////////////////////////////////////////////////////////////////
// Email Input Utility Functions
////////////////////////////////////////////////////////////////////////////

#define REGISTER_OK (0)
#define REGISTER_ERR_INVALID_EMAIL (-1)
#define REGISTER_ERR_EMAILDB (-2)
#define REGISTER_ERR_TOO_MANY_ACCOUNTS (-3)
#define REGISTER_ERR_CANCEL (-4)

static int
register_email_input(const userec_t *u, char *email);

static int
register_count_email(const userec_t *u, const char *email);

static int
register_check_and_update_emaildb(const userec_t *u, const char *email);

////////////////////////////////////////////////////////////////////////////
// Value Validation
////////////////////////////////////////////////////////////////////////////
static int
HaveRejectStr(const char *s, const char *fn_db)
{
    FILE *fp = fopen(fn_db, "r");
    char buf[256];
    int ret = 0;

    if (!fp)
        return 0;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        // strip \n
        buf[strlen(buf) - 1] = 0;
        if (*buf && DBCS_strcasestr(s, buf)) {
            ret = 1;
            break;
        }
    }
    fclose(fp);
    return ret;
}

static int
strlen_without_space(const char *s)
{
    int i = 0;
    while (*s)
	if (*s++ != ' ') i++;
    return i;
}


int
reserved_user_id(const char *userid)
{
    if (file_exist_record(FN_CONF_RESERVED_ID, userid))
       return 1;
    return 0;
}

int
bad_user_id(const char *userid)
{
    if(!is_validuserid(userid))
	return 1;

#if defined(STR_REGNEW)
    if (strcasecmp(userid, STR_REGNEW) == 0)
	return 1;
#endif

#if defined(STR_GUEST) && !defined(NO_GUEST_ACCOUNT_REG)
    if (strcasecmp(userid, STR_GUEST) == 0)
	return 1;
#endif
    return 0;
}

static char *
isvalidname(char *rname)
{
#ifdef DBG_DISABLE_CHECK
    return NULL;
#endif // DBG_DISABLE_CHECK

    if (strlen_without_space(rname) < 4 ||
        HaveRejectStr(rname, FN_REJECT_STR_NAME))
	return "您的輸入似乎不正確";

    return NULL;
}

static char *
isvalidcareer(char *career)
{
#ifdef DBG_DISABLE_CHECK
    return NULL;
#endif // DBG_DISABLE_CHECK

    if (strlen_without_space(career) < 6)
	return "您的輸入似乎不正確";

    if (HaveRejectStr(career, FN_REJECT_STR_CAREER))
	return "您的輸入似乎有誤";

    return NULL;
}

static char *
isvalidaddr(char *addr, int isForeign)
{
#ifdef DBG_DISABLE_CHECK
    return NULL;
#endif // DBG_DISABLE_CHECK

    // addr[0] > 0: check if address is starting by Chinese.
    if (DBCS_strcasestr(addr, "信箱") != 0 ||
        DBCS_strcasestr(addr, "郵政") != 0)
	return "抱歉我們不接受郵政信箱";

    if (strlen_without_space(addr) < 15)
	return "這個地址似乎並不完整";

    if (!isForeign &&
	((DBCS_strcasestr(addr, "市") == 0 &&
	  DBCS_strcasestr(addr, "巿") == 0 &&
	  DBCS_strcasestr(addr, "縣") == 0 &&
	  DBCS_strcasestr(addr, "室") == 0) ||
	 strcmp(&addr[strlen(addr) - 2], "段") == 0 ||
	 strcmp(&addr[strlen(addr) - 2], "路") == 0 ||
	 strcmp(&addr[strlen(addr) - 2], "巷") == 0 ||
	 strcmp(&addr[strlen(addr) - 2], "弄") == 0 ||
	 strcmp(&addr[strlen(addr) - 2], "區") == 0 ||
	 strcmp(&addr[strlen(addr) - 2], "市") == 0 ||
	 strcmp(&addr[strlen(addr) - 2], "街") == 0))
	return "這個地址似乎並不完整";

    if (HaveRejectStr(addr, FN_REJECT_STR_ADDR))
	return "這個地址似乎有誤";

    return NULL;
}

////////////////////////////////////////////////////////////////////////////
// Account Expiring
////////////////////////////////////////////////////////////////////////////

/* -------------------------------- */
/* New policy for allocate new user */
/* (a) is the worst user currently  */
/* (b) is the object to be compared */
/* -------------------------------- */
static int
compute_user_value(const userec_t * urec, time4_t clock)
{
    int             value;

    /* if (urec) has XEMPT permission, don't kick it */
    if ((urec->userid[0] == '\0') || (urec->userlevel & PERM_XEMPT)
    /* || (urec->userlevel & PERM_LOGINOK) */
	|| !strcmp(STR_GUEST, urec->userid))
	return 999999;
    value = (clock - urec->lastlogin) / 60;	/* minutes */

#ifdef STR_REGNEW
    /* new user should register in 30 mins */
    // XXX 目前 new acccount 並不會在 utmp 裡放 STR_REGNEW...
    if (strcmp(urec->userid, STR_REGNEW) == 0)
	return 30 - value;
#endif

    return ((urec->userlevel & (PERM_LOGINOK|PERM_VIOLATELAW)) ?
            KEEP_DAYS_REGGED : KEEP_DAYS_UNREGGED) * 24 * 60 - value;
}

int
check_and_expire_account(int uid, const userec_t * urec, int expireRange)
{
    char            genbuf[200];
    int             val;
    if ((val = compute_user_value(urec, now)) < 0) {
	snprintf(genbuf, sizeof(genbuf), "#%d %-12s %s %d %d %d",
		 uid, urec->userid, Cdatelite(&(urec->lastlogin)),
		 urec->numlogindays, urec->numposts, val);

	// 若超過 expireRange 則砍人，
	// 不然就 return 0
	if (-val > expireRange)
	{
	    log_usies("DATED", genbuf);
	    // log_usies("CLEAN", genbuf);
	    kill_user(uid, urec->userid);
	} else val = 0;
    }
    return val;
}

////////////////////////////////////////////////////////////////////////////
// Regcode Support
////////////////////////////////////////////////////////////////////////////

#define REGCODE_INITIAL "v6" // always 2 characters
#define REGCODE_LEN     (13)
#define REGCODE_SZ      (REGCODE_LEN + 1)

static char *
getregfile(char *buf)
{
    // not in user's home because s/he could zip his/her home
    snprintf(buf, PATHLEN, FN_JOBSPOOL_DIR ".regcode.%s", cuser.userid);
    return buf;
}

static bool
saveregcode(const char *regcode)
{
    char    fpath[PATHLEN];
    int     fd;

    getregfile(fpath);
    if((fd = OpenCreate(fpath, O_WRONLY)) == -1){
	perror("open");
	return false;
    }
    if (write(fd, regcode, strlen(regcode)) < 0) {
	close(fd);
	return false;
    }
    close(fd);
    return true;
}

static void
makeregcode(char *buf)
{
    int     i;
    // prevent ambigious characters: oOlI
    const char *alphabet = "qwertyuipasdfghjkzxcvbnmoQWERTYUPASDFGHJKLZXCVBNM";

    /* generate a new regcode */
    buf[REGCODE_LEN] = 0;
    buf[0] = REGCODE_INITIAL[0];
    buf[1] = REGCODE_INITIAL[1];
    for( i = 2 ; i < REGCODE_LEN ; ++i )
	buf[i] = alphabet[random() % strlen(alphabet)];
}

// getregcode reads the register code from jobspool directory. If found,
// regcode is copied to buf and 0 is returned. Otherwise, an empty string is
// copied to buf and non-zero is returned.
static int
getregcode(char *buf)
{
    int     fd;
    char    fpath[PATHLEN];

    getregfile(fpath);
    if( (fd = open(fpath, O_RDONLY)) == -1 ){
	buf[0] = 0;
	return -1;
    }
    int r = read(fd, buf, REGCODE_LEN);
    close(fd);
    buf[REGCODE_LEN] = 0;
    return r == REGCODE_LEN ? 0 : -1;
}

void
delregcodefile(void)
{
    char    fpath[PATHLEN];
    getregfile(fpath);
    unlink(fpath);
}


////////////////////////////////////////////////////////////////////////////
// Justify Utilities
////////////////////////////////////////////////////////////////////////////

static void email_regcode(const char *regcode, const char *email)
{
    char buf[256];

    /*
     * It is intended to use BBSENAME instead of BBSNAME here.
     * Because recently many poor users with poor mail clients
     * (or evil mail servers) cannot handle/decode Chinese
     * subjects (BBSNAME) correctly, so we'd like to use
     * BBSENAME here to prevent subject being messed up.
     * And please keep BBSENAME short or it may be truncated
     * by evil mail servers.
     */
    snprintf(buf, sizeof(buf), " " BBSENAME " - [ %s ]", regcode);

    bsmtp("etc/registermail", buf, email, "non-exist");
}

static void
email_justify(const userec_t *muser)
{
	char regcode[REGCODE_SZ];

	makeregcode(regcode);
	if (!saveregcode(regcode))
	    exit(1);

	email_regcode(regcode, muser->email);

        move(20,0);
        clrtobot();
	outs("我們即將寄出認證信 (您應該會在數分鐘到數小時內收到)\n"
	     "收到後您可以根據認證信標題的認證碼\n"
	     "輸入到 (U)ser -> (R)egister 後就可以完成註冊");
	pressanykey();
	return;
}


/* 使用者填寫註冊表格 */
static void
getfield(int line, const char *info, const char *notes_fn, const char *desc, char *buf, int len)
{
    char            prompt[STRLEN];
    char            genbuf[PATHLEN];

    // clear first
    move(line, 0); clrtobot();
    // notes appear in line+3 (+0=msg, +1=input, +2=blank)
    if (dashs(notes_fn) > 0 && (line+3) < b_lines )
    {
	show_file(notes_fn, line+3, t_lines - (line+3), SHOWFILE_ALLOW_ALL);
    }
    move(line, 0); prints("  原先設定：%-30.30s (%s)", buf, info);
    snprintf(prompt, sizeof(prompt),
	    ANSI_COLOR(1) ">>%s" ANSI_RESET "：",
	    desc);
    if (getdata_str(line + 1, 0, prompt, genbuf, len, DOECHO, buf))
	strcpy(buf, genbuf);
    move(line, 0); clrtobot();
    prints("  %s：%s\n", desc, buf);
}


int
setupnewuser(const userec_t *user)
{
    char            genbuf[50];
    char           *fn_fresh = ".fresh";
    userec_t        utmp;
    time_t          clock;
    struct stat     st;
    int             fd, uid;

    clock = now;

    // XXX race condition...
    if (dosearchuser(user->userid, NULL))
    {
	vmsg("手腳不夠快，別人已經搶走了！");
	exit(1);
    }

    /* Lazy method : 先找尋已經清除的過期帳號 */
    if ((uid = dosearchuser("", NULL)) == 0) {
	/* 每 1 個小時，清理 user 帳號一次 */
	if ((stat(fn_fresh, &st) == -1) || (st.st_mtime < clock - 3600)) {
	    if ((fd = OpenCreate(fn_fresh, O_RDWR)) == -1)
		return -1;
	    write(fd, ctime(&clock), 25);
	    close(fd);
	    log_usies("CLEAN", "dated users");

	    fprintf(stdout, "尋找新帳號中, 請稍待片刻...\n\r");

	    if ((fd = OpenCreate(fn_passwd, O_RDWR)) == -1)
		return -1;

	    /* 不曉得為什麼要從 2 開始... Ptt:因為SYSOP在1 */
	    for (uid = 2; uid <= MAX_USERS; uid++) {
		passwd_sync_query(uid, &utmp);
		// tolerate for one year.
		check_and_expire_account(uid, &utmp, 365*12*60);
	    }
	}
    }

    /* initialize passwd semaphores */
    if (passwd_init())
	exit(1);

    passwd_lock();

    uid = dosearchuser("", NULL);
    if ((uid <= 0) || (uid > MAX_USERS)) {
	passwd_unlock();
	vmsg("抱歉，本站使用者帳號總數已達上限，暫時無法註冊新帳號。");
	exit(1);
    }

    setuserid(uid, user->userid);
    snprintf(genbuf, sizeof(genbuf), "uid %d", uid);
    log_usies("APPLY", genbuf);

    SHM->money[uid - 1] = user->money;

    if (passwd_sync_update(uid, (userec_t *)user) == -1) {
	passwd_unlock();
	vmsg("客滿了，再見！");
	exit(1);
    }

    passwd_unlock();

    return uid;
}

int
query_adbanner_usong_pref_changed(const userec_t *u, char force_yn)
{
    char old = (u->uflag & UF_ADBANNER_USONG) ? 1 : 0,
	 new = 0,
	 defyes = 0,
	 ans = 1;

    assert(u);
    if ( !(u->uflag & UF_ADBANNER) )
	return 0;

#ifdef ADBANNER_DEFAULT_YES
    defyes = ADBANNER_DEFAULT_YES;
#endif
    vs_hdr("動態看板心情點播顯示設定");
    // draw a box here
    outs(
	"\n\n\t在使用 BBS 的過程中，您可能會在畫面上方此區看到一些動態的訊息告示，"
	"\n\n\t其內容開放給各使用者與公益團體申請，所以會包含非商業的活動資訊/網宣，"
	"\n\n\t還有來自各使用者的心情點播 (可能包含該使用者的政治性言論或各種留言)。"
	"\n\n\n\n"
	"\n\n\t" ANSI_COLOR(1)
	"此類由使用者自行發表的文字與圖像並不代表站方立場。" ANSI_RESET
	"\n\n\t由於心情點播部份較難定義出完整的審核標準，為了避免造成閱讀者的不快或"
	"\n\n\t誤會，在此要確認您是否希望顯示心情點播內容。"
	"\n\n\t(若之後想改變此類設定，可至 (U)個人設定區->(U)個人化設定 調整)\n");
    vs_rectangle_simple(1, 1, 78, MAX_ADBANNER_HEIGHT);

    do {
	// alert if not first rounod
	if (ans != 1) { move(b_lines-2, 0); outs("請確實輸入 y 或 n。"); bell(); }
	ans = vansf("請問您希望在動態告示區看到來自其它使用者的心情點播嗎? %s: ",
		force_yn ? "[y/n]" : defyes ? "[Y/n]" : "[y/N]");

	// adjust answers
	if (!force_yn)
	{
	    if (defyes && ans != 'n')
		ans = 'y';
	    else if (!defyes && ans != 'y')
		ans = 'n';
	}
    } while (ans != 'y' && ans != 'n');

    if (ans == 'y')
	new = 1;
    else
	new = 0;

    return (new != old) ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////
// User Agreement Version
/////////////////////////////////////////////////////////////////////////////

static uint8_t
get_system_user_agreement_version()
{
    unsigned int version = 0;
#ifdef HAVE_USERAGREEMENT_VERSION
    FILE *fp = fopen(HAVE_USERAGREEMENT_VERSION, "r");
    if (fp != NULL) {
	fscanf(fp, "%u", &version);
	fclose(fp);
    }
    if (version > 255)
	version = 0;
#endif
    return version;
}

#define UAVER_OK (0)
#define UAVER_UNKNOWN (-1)
#define UAVER_OUTDATED (-2)

static int
check_user_agreement_version(uint8_t version)
{
#ifdef HAVE_USERAGREEMENT_ACCEPTABLE
    FILE *fp = fopen(HAVE_USERAGREEMENT_ACCEPTABLE, "r");
    if (fp == NULL)
	return UAVER_UNKNOWN;

    int result = UAVER_OUTDATED;
    unsigned int acceptable;
    while (fscanf(fp, "%u", &acceptable) == 1) {
	if (version == acceptable) {
	    result = UAVER_OK;
	    break;
	}
    }
    fclose(fp);
    return result;
#else
    return UAVER_OK;
#endif
}

static int
accept_user_aggrement()
{
#ifdef HAVE_USERAGREEMENT
    int haveag = more(HAVE_USERAGREEMENT, YEA);
    while (haveag != -1) {
	int c = vans("請問您接受這份使用者條款嗎? (yes/no) ");
	if (c == 'y')
	    break;
	else if (c == 'n')
	    return 0;
	vmsg("請輸入 y表示接受, n表示不接受");
    }
#endif
    return 1;
}

void
ensure_user_agreement_version()
{
    switch (check_user_agreement_version(cuser.ua_version)) {
	case UAVER_OK:
	    return;

	case UAVER_UNKNOWN:
	    vmsg("系統錯誤, 暫時無法登入");
	    break;

	case UAVER_OUTDATED:
	    vmsg("使用者條款已經更新, 請重新檢視.");
	    uint8_t version = get_system_user_agreement_version();
	    if (!accept_user_aggrement()) {
		vmsg("抱歉, 您須要接受使用者條款才能繼續使用我們的服務唷!");
		break;
	    }
	    pwcuSetUserAgreementVersion(version);
	    return;
    }
    exit(1);
}

static const char *
do_register_captcha()
{
#ifdef USE_REG_CAPTCHA
#ifndef USE_REMOTE_CAPTCHA
#error "To use USE_REG_CAPTCHA, you must also set USE_REMOTE_CAPTCHA"
#endif
    return remote_captcha();
#else
    return NULL;
#endif
}

static bool
query_yn(int y, const char *msg)
{
    int try = 0;
    do {
        char ans[3];
	if (++try > 10) {
	    vmsg(MSG_ERR_MAXTRIES);
	    exit(1);
	}
        getdata(y, 0, msg, ans, 3, LCECHO);
        if (ans[0] == 'y')
	    return true;
        if (ans[0] == 'n')
	    return false;
        bell();
    } while (1);
}

/////////////////////////////////////////////////////////////////////////////
// New Registration (Phase 1: Create Account)
/////////////////////////////////////////////////////////////////////////////

static int
new_register_email_verify(char *email)
{
    clear();
    vs_hdr("E-Mail 認證");
    move(1, 0);
    outs("您好, 本站要求註冊時進行 E-Mail 認證:\n"
	 "  請輸入您的 E-Mail , 我們會寄發含有認證碼的信件給您\n"
	 "  收到後請到請輸入認證碼, 即可進行註冊\n"
	 "  註: 本站不接受 yahoo, kimo等免費的 E-Mail\n"
	 "\n"
	 "**********************************************************\n"
	 "* 若過久未收到請到郵件垃圾桶檢查是否被當作垃圾信(SPAM)了,*\n"
	 "* 另外若輸入後發生認證碼錯誤請先確認輸入是否為最後一封   *\n"
	 "* 收到的認證信，若真的仍然不行請再重填一次 E-Mail.       *\n"
	 "**********************************************************\n");

    // Get a valid email from user.
    int err;
    int tries = 0;
    do {
	if (++tries > 10) {
	    return REGISTER_ERR_CANCEL;
	}

	err = register_email_input(NULL, email);
	switch (err) {
	    case REGISTER_OK:
		if (strcasecmp(email, "x") != 0)
		    break;
		// User input is "x".
		err = REGISTER_ERR_INVALID_EMAIL;
		// fallthrough
	    case REGISTER_ERR_INVALID_EMAIL:
	    case REGISTER_ERR_CANCEL:
		move(15, 0); clrtobot();
		move(17, 0);
		outs("指定的 E-Mail 不正確。可能你輸入的是免費的Email，\n");
		outs("或曾有使用者以本 E-Mail 認證後被取消資格。\n\n");
		pressanykey();
		continue;

	    case REGISTER_ERR_TOO_MANY_ACCOUNTS:
		move(15, 0); clrtobot();
		move(17, 0);
		outs("指定的 E-Mail 已註冊過多帳號, 請使用其他 E-Mail.\n");
		pressanykey();
		continue;

	    default:
		return err;
	}
    } while (err != REGISTER_OK);

    // Send and check regcode.
    char inregcode[REGCODE_SZ] = {0}, regcode[REGCODE_SZ];
    char buf[80];
    makeregcode(regcode);

    tries = 0;
    int num_sent = 0;
    bool send_code = true;
    while (1) {
	if (++tries > 10) {
	    vmsg("錯誤次數過多, 請稍後再試");
	    return REGISTER_ERR_CANCEL;
	}
	if (send_code) {
	    tries++;  // Expensive operation.
	    email_regcode(regcode, email);
	    send_code = false;
	    num_sent++;
	    snprintf(buf, sizeof(buf),
		    ANSI_COLOR(1;31) " (第 %d 次)" ANSI_RESET, num_sent);
	}

	move(15, 0); clrtobot();
	outs("認證碼已寄送至:");
	if (num_sent > 1) outs(buf);
	outs("\n");
	outs(ANSI_COLOR(1;33) "  ");
	outs(email);
	outs(ANSI_RESET "\n");
	outs("認證碼總共應有十三碼, 沒有空白字元.\n");
	outs("若過久沒收到認證碼可輸入 x 重新寄送.\n");

	getdata(20, 0, "請輸入認證碼：",
		inregcode, sizeof(inregcode), DOECHO);
	if (strcasecmp(inregcode, "x") == 0) {
	    send_code = true;
	    continue;
	}
	if (strcasecmp(inregcode, regcode) != 0) {
	    vmsg("認證碼錯誤");
	    continue;
	}
	break;
    }
    return REGISTER_OK;
}

void
new_register(void)
{
    userec_t        newuser;
    char            passbuf[STRLEN];
    int             try, id, uid;
    const char 	   *errmsg = NULL;
    uint8_t         ua_version = get_system_user_agreement_version();
    bool	    email_verified = false;

#ifdef REQUIRE_SECURE_CONN_TO_REGISTER
    if (!mbbsd_is_secure_connection()) {
	vmsg("請使用安全連線註冊帳號!");
	exit(1);
    }
#endif

    if (!accept_user_aggrement()) {
	vmsg("抱歉, 您須要接受使用者條款才能註冊帳號享受我們的服務唷!");
	exit(1);
    }

    errmsg = do_register_captcha();
    if (errmsg) {
	vmsg(errmsg);
	exit(1);
    }

    // setup newuser
    memset(&newuser, 0, sizeof(newuser));
    newuser.version = PASSWD_VERSION;
    newuser.userlevel = PERM_DEFAULT;
    newuser.uflag = UF_BRDSORT | UF_ADBANNER | UF_CURSOR_ASCII;
    newuser.firstlogin = newuser.lastlogin = now;
    newuser.pager = PAGER_ON;
    newuser.numlogindays = 1;
    newuser.ua_version = ua_version;
    strlcpy(newuser.lasthost, fromhost, sizeof(newuser.lasthost));

#ifdef DBCSAWARE
    newuser.uflag |= UF_DBCS_AWARE | UF_DBCS_DROP_REPEAT;
#endif

#ifdef REQUIRE_VERIFY_EMAIL_AT_REGISTER
    if (new_register_email_verify(newuser.email) == REGISTER_OK) {
	email_verified = true;
    } else {
	exit(1);
    }
#endif

#ifdef UF_ADBANNER_USONG
    if (query_adbanner_usong_pref_changed(&newuser, 0))
	newuser.uflag |= UF_ADBANNER_USONG;
#endif


    more("etc/register", NA);
    try = 0;
    while (1) {
        userec_t xuser;
	int minute;

	if (++try >= 6) {
	    vmsg(MSG_ERR_MAXTRIES);
	    exit(1);
	}
	getdata(17, 0, msg_uid, newuser.userid,
		sizeof(newuser.userid), DOECHO);
        strcpy(passbuf, newuser.userid);

	if (bad_user_id(passbuf))
	    outs("無法接受這個代號，請使用英文字母，並且不要包含空格\n");
	else if ((id = getuser(passbuf, &xuser)) &&
		// >=: see check_and_expire_account definition
		 (minute = check_and_expire_account(id, &xuser, 0)) >= 0)
	{
            // XXX Magic number >= MAX_USERS: never expires.
            // Probably because of sysadmin perms, or due to violation.
	    if (minute == 999999)
		outs("此代號已經有人使用，請使用別的代號\n");
	    else {
		prints("此代號已經有人使用 還有 %d 天才過期\n",
			minute / (60 * 24) + 1);
	    }
	}
	else if (reserved_user_id(passbuf))
	    outs("此代號已由系統保留，請使用別的代號\n");
#if !defined(NO_CHECK_AMBIGUOUS_USERID) && defined(USE_REGCHECKD)
	// XXX if we check id == 0 here, replacing an expired id will be delayed.
	else if (/*id == 0 && */
		 regcheck_ambiguous_userid_exist(passbuf) > 0) // ignore if error occurs
	    outs("此代號過於近似它人帳號，請改用別的代號。\n");
#endif
#ifdef MIN_ALLOWED_ID_LEN
        else if (strlen(passbuf) < MIN_ALLOWED_ID_LEN)
	    prints("代號過短，請使用 %d 個字元以上的代號\n", MIN_ALLOWED_ID_LEN);
#endif
	else // success
	    break;
    }

    // XXX 記得最後 create account 前還要再檢查一次 acc

    try = 0;
    while (1) {
	if (++try >= 6) {
	    vmsg(MSG_ERR_MAXTRIES);
	    exit(1);
	}
	move(20, 0); clrtoeol();
	outs(ANSI_COLOR(1;33)
    "為避免被偷看，您的密碼會顯示為 * ，直接輸入完後按 Enter 鍵即可。\n"
    "另外請注意密碼只有前八個字元有效，超過的將自動忽略。"
	ANSI_RESET);
        if ((getdata(18, 0, "請設定密碼：", passbuf, PASS_INPUT_LEN + 1,
                     PASSECHO) < 3) ||
	    !strcmp(passbuf, newuser.userid)) {
	    outs("密碼太簡單，易遭入侵，至少要 4 個字，請重新輸入\n");
	    continue;
	}
	strlcpy(newuser.passwd, passbuf, PASSLEN);
	getdata(19, 0, "請確認密碼：", passbuf, PASS_INPUT_LEN + 1, PASSECHO);
	if (strncmp(passbuf, newuser.passwd, PASSLEN)) {
	    move(19, 0);
	    outs("設定與確認時輸入的密碼不一致, 請重新輸入密碼.\n");
	    continue;
	}
	passbuf[8] = '\0';
	strlcpy(newuser.passwd, genpasswd(passbuf), PASSLEN);
	break;
    }

    // set-up more information.
    int y = 17;
    move(y, 0); clrtobot();
    outs("[ " ANSI_COLOR(1;33));
    outs(newuser.userid);
    outs(ANSI_RESET " ] 您好，請填寫您的個人資料。");
    y++;

    // warning: because currutmp=NULL, we can simply pass newuser.* to getdata.
    // DON'T DO THIS IF YOUR currutmp != NULL.
    try = 0;
    while (strlen(newuser.nickname) < 2)
    {
	if (++try > 10) {
	    vmsg(MSG_ERR_MAXTRIES);
	    exit(1);
	}
	getdata(y, 0, "綽號暱稱：", newuser.nickname,
		sizeof(newuser.nickname), DOECHO);
    }

    try = 0;
    y++;
    while (strlen(newuser.realname) < 4)
    {
	if (++try > 10) {
	    vmsg(MSG_ERR_MAXTRIES);
	    exit(1);
	}
	getdata(y, 0, "真實姓名：", newuser.realname,
		sizeof(newuser.realname), DOECHO);

	if ((errmsg = isvalidname(newuser.realname))) {
	    memset(newuser.realname, 0, sizeof(newuser.realname));
	    vmsg(errmsg);
	}
    }

    try = 0;
    y++;
    while (strlen(newuser.career) < 6)
    {
	if (++try > 10) {
	    vmsg(MSG_ERR_MAXTRIES);
	    exit(1);
	}
	getdata(y, 0, "服務單位：", newuser.career,
		sizeof(newuser.career), DOECHO);

	if ((errmsg = isvalidcareer(newuser.career))) {
	    memset(newuser.career, 0, sizeof(newuser.career));
	    vmsg(errmsg);
	}
    }

    try = 0;
    y++;
    while (strlen(newuser.address) < 8)
    {
	if (++try > 10) {
	    vmsg(MSG_ERR_MAXTRIES);
	    exit(1);
	}
	getdata(y, 0, "聯絡地址：", newuser.address,
		sizeof(newuser.address), DOECHO);

        // We haven't ask isForeign yet, so assume that's one (for lesser check)
	if ((errmsg = isvalidaddr(newuser.address, 1))) {
	    memset(newuser.address, 0, sizeof(newuser.address));
	    vmsg(errmsg);
	}
    }

    // Over 18.
    y++;
    mvouts(y, 0, "本站部份看板可能有限制級內容只適合成年人士閱讀。");
    if (query_yn(y + 1,
		"您是否年滿十八歲並同意觀看此類看板(若否請輸入n)? [y/n]:"))
	newuser.over_18 = 1;

    // Whether to limit login to secure connection only.
    if (mbbsd_is_secure_connection()) {
	// Screen full.
	y = 17;
	move(y, 0); clrtobot();
	outs("[ 連線設定 ]");

	y++;
	if (query_yn(y, "您是否要限制此帳號僅能使用安全連線登入? [y/n]:"))
	    newuser.uflag |= UF_SECURE_LOGIN;
    }

#ifdef REGISTER_VERIFY_CAPTCHA
    if (!verify_captcha("為了繼續您的註冊程序\n"))
    {
	vmsg(MSG_ERR_MAXTRIES);
	exit(1);
    }
#endif

    setupnewuser(&newuser);

    if( (uid = initcuser(newuser.userid)) < 0) {
	vmsg("無法建立帳號");
	exit(1);
    }
    log_usies("REGISTER", fromhost);

    // 確實的把舊資料清掉
    {
        char home[PATHLEN], tmp[PATHLEN];
        syncnow();
        sethomepath(home, newuser.userid);
        sprintf(tmp, "tmp/%s.%d", newuser.userid, now);
        if (dashd(home) && Rename(home, tmp) != 0) {
            // failed to active account.
            pwcuBitDisableLevel(PERM_BASIC);
            log_filef("log/home_fail.log", LOG_CREAT,
                      "%s: failed to remove.\n", newuser.userid);
            vmsg("抱歉，系統出錯，此帳號已鎖定。");
            exit(0);
        }
    }

#ifdef USE_REMOVEBM_ON_NEWREG
    {
        char buf[PATHLEN];
        snprintf(buf, sizeof(buf),
                 BBSHOME "/bin/removebm '%s' >>"
                 BBSHOME "/log/removebm.log 2>&1",
                 newuser.userid);
        system(buf);
    }
#endif

    // Mark register complete after the user has been created.
    if (email_verified) {
	if (register_check_and_update_emaildb(&newuser, newuser.email) ==
		REGISTER_OK) {
	    char justify[sizeof(newuser.justify)];
	    snprintf(justify, sizeof(justify), "<E-Mail>: %s", Cdate(&now));
	    pwcuRegCompleteJustify(justify);
	} else {
	    vmsg("Email 認證設定失敗, 請稍後自行再次填寫註冊單");
	}
    }
}

bool
check_email_allow_reject_lists(char *email, const char **errmsg, const char **notice_file)
{
    FILE           *fp;
    char            buf[128], *c;

    if (errmsg)
	*errmsg = NULL;
    if (notice_file)
	*notice_file = NULL;

    c = strchr(email, '@');

    // reject no '@' or multiple '@'
    if (c == NULL || c != strrchr(email, '@'))
    {
	if (errmsg) *errmsg = "E-Mail ���榡�����T�C";
	return false;
    }

    // domain tolower
    str_lower(c, c);

    // allow list
    bool allow = false;
    if ((fp = fopen("etc/whitemail", "rt")))
    {
	while (fgets(buf, sizeof(buf), fp)) {
	    if (buf[0] == '#')
		continue;
	    chomp(buf);
	    c = buf+1;
	    // vmsgf("%c %s %s",buf[0], c, email);
	    switch(buf[0])
	    {
		case 'A': if (strcasecmp(c, email) == 0)	allow = true; break;
		case 'P': if (strcasestr(email, c))	allow = true; break;
		case 'S': if (strcasecmp(strstr(email, "@") + 1, c) == 0) allow = true; break;
		case '%': allow = true; break; // allow all
	        // domain match (*@c | *@*.c)
		case 'D': if (strlen(email) > strlen(c))
			  {
			      // c2 points to starting of possible c.
			      const char *c2 = email + strlen(email) - strlen(c);
			      if (strcasecmp(c2, c) != 0)
				  break;
			      c2--;
			      if (*c2 == '.' || *c2 == '@')
				  allow = true;
			  }
			  break;
	    }
	    if (allow) break;
	}
	fclose(fp);
	if (!allow)
	{
	    if (notice_file && dashf(FN_NOTIN_WHITELIST_NOTICE))
		*notice_file = FN_NOTIN_WHITELIST_NOTICE;
	    if (errmsg)
		*errmsg = "��p�A�ثe�������� Email �����U�ӽСC";
	    return false;
	}
    }

    // reject list
    allow = true;
    if ((fp = fopen("etc/banemail", "r"))) {
	while (allow && fgets(buf, sizeof(buf), fp)) {
	    if (buf[0] == '#')
		continue;
	    chomp(buf);
	    c = buf+1;
	    switch(buf[0])
	    {
		case 'A': if (strcasecmp(c, email) == 0)
			  {
			      allow = false;
			      // exact match
			      if (errmsg)
				  *errmsg = "���q�l�H�c�w�Q�T����U";
			  }
			  break;
		case 'P': if (strcasestr(email, c))
			  {
			      allow = false;
			      if (errmsg)
				*errmsg = "���H�c�w�Q�T��Ω���U (�i��O�K�O�H�c)";
			  }
			  break;
		case 'S': if (strcasecmp(strstr(email, "@") + 1, c) == 0)
			  {
			      allow = false;
			      if (errmsg)
				  *errmsg = "���H�c�w�Q�T��Ω���U (�i��O�K�O�H�c)";
			  }
			  break;
		case 'D': if (strlen(email) > strlen(c))
			  {
			      // c2 points to starting of possible c.
			      const char *c2 = email + strlen(email) - strlen(c);
			      if (strcasecmp(c2, c) != 0)
				  break;
			      c2--;
			      if (*c2 == '.' || *c2 == '@')
			      {
				  allow = false;
				  if (errmsg)
				      *errmsg = "���H�c������w�Q�T��Ω���U (�i��O�K�O�H�c)";
			      }
			  }
			  break;
	    }
	}
	fclose(fp);
    }
    return allow;
}

int
check_regmail(char *email)
{
    const char *errmsg, *notice_file;
    bool allow = check_email_allow_reject_lists(email, &errmsg, &notice_file);
    if (allow)
	return 1;

    // show whitemail notice if it exists.
    if (notice_file) {
	VREFSCR scr = vscr_save();
	more(notice_file, NA);
	pressanykey();
	vscr_restore(scr);
    } else if (errmsg) {
	vmsg(errmsg);
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// User Registration (Phase 2: Validation)
/////////////////////////////////////////////////////////////////////////////

void
check_register(void)
{
    char fn[PATHLEN];

    // 已經通過的就不用了
    if (HasUserPerm(PERM_LOGINOK) || HasUserPerm(PERM_SYSOP))
	return;

    // 基本權限被拔應該是要讓他不能註冊用。
    if (!HasUserPerm(PERM_BASIC))
	return;

    /*
     * 避免使用者被退回註冊單後，在知道退回的原因之前，
     * 又送出一次註冊單。
     */
    setuserfile(fn, FN_REJECT_NOTIFY);
    if (dashf(fn))
    {
	int xun = 0, abort = 0;
	userec_t u = {0};
	char buf[PATHLEN] = "";
	FILE *fp = fopen(fn, "rt");

	// load reference
	fgets(buf, sizeof(buf), fp);
	fclose(fp);

	// parse reference
	if (buf[0] == '#')
	{
	    xun = atoi(buf+1);
	    if (xun <= 0 || xun > MAX_USERS ||
		passwd_sync_query(xun, &u) < 0 ||
		!(u.userlevel & (PERM_ACCOUNTS | PERM_ACCTREG)))
		memset(&u, 0, sizeof(u));
	    // now u is valid only if reference is loaded with account sysop.
	}

	// show message.
	more(fn, YEA);
	move(b_lines-4, 0); clrtobot();
	outs("\n" ANSI_COLOR(1;31)
	     "前次註冊單審查失敗。 (本記錄已備份於您的信箱中)\n"
	     "請重新申請並照上面指示正確填寫註冊單。\n");

	if (u.userid[0])
	    outs("如有任何問題或需要與站務人員聯絡請按 r 回信。");

	outs(ANSI_RESET "\n");


	// force user to confirm.
	while (!abort)
	{
	    switch(vans(u.userid[0] ?
		    "請輸入 y 繼續或輸入 r 回信給站務: " :
		    "請輸入 y 繼續: "))
	    {
		case 'y':
		    abort = 1;
		    break;

		case 'r':
		    if (!u.userid[0])
			break;

		    // mail to user
		    setuserfile(quote_file, FN_REJECT_NOTIFY);
		    strlcpy(quote_user, "[退註通知]", sizeof(quote_user));
		    clear();
		    do_innersend(u.userid, NULL, "[註冊問題] 退註相關問題", NULL);
		    abort = 1;
		    // quick return to avoid confusing user
		    unlink(fn);
		    return;
		    break;

		default:
		    bell();
		    break;
	    }
	}

	unlink(fn);
    }

    // 只有以下情形需要自動叫出註冊選單:
    // 1. 首次註冊 (numlogindays < 2)
    // 2. 正在 e-mail 認證
    // 3. 申請板主然後被取消(email)認證
    // 其它情形就假設 user 不想註冊了

    setuserfile(fn, FN_REJECT_NOTIFY);
    if ((cuser.numlogindays < 2) ||
        HasUserPerm(PERM_NOREGCODE) ||
        strstr(cuser.email, "@")) {
        clear();
        vs_hdr2(" 未完成註冊認證 ", " 您的帳號尚未完成認證");
        move(9, 0);
        outs("  您目前尚未通過註冊認證程序，請細詳填寫"
             ANSI_COLOR(32) "註冊申請單" ANSI_RESET "，\n"
             "  通告站長以獲得進階使用權力。\n\n");
        outs("  如果您之前曾使用 email 等認證方式通過註冊認證但又看到此訊息，\n"
             "  代表您的認證由於資料不完整已被取消 (常見於申請開新看板的板主)。\n");
        u_register();
    }
}

static int
create_regform_request()
{
    FILE *fn;

    char fname[PATHLEN];
    setuserfile(fname, FN_REGFORM);
    fn = fopen(fname, "wt");	// regform 2: replace model

    if (!fn)
	return 0;

    // create request data
    fprintf(fn, "uid: %s\n",    cuser.userid);
    fprintf(fn, "name: %s\n",   cuser.realname);
    fprintf(fn, "career: %s\n", cuser.career);
    fprintf(fn, "addr: %s\n",   cuser.address);
    fprintf(fn, "email: %s\n",  "x"); // email is apparently 'x' here.
    fprintf(fn, "----\n");
    fclose(fn);

    // regform2 must update request list
    file_append_record(FN_REQLIST, cuser.userid);

    // save justify information
    pwcuRegSetTemporaryJustify("<Manual>", "x");
    return 1;
}

static int
register_email_input(const userec_t *u, char *email)
{
    while (1) {
	email[0] = 0;
	getfield(15, "身分認證用", REGNOTES_ROOT "email", "E-Mail Address", email, EMAILSZ);
	strip_blank(email, email);
	if (strlen(email) == 0)
	    return REGISTER_ERR_CANCEL;
	if (strcmp(email, "X") == 0) email[0] = 'x';
	if (strcmp(email, "x") == 0)
	    return REGISTER_OK;

	// before long waiting, alert user
	move(18, 0); clrtobot();
	outs("正在確認 email, 請稍候...\n");
	doupdate();

	if (!check_regmail(email))
	    return REGISTER_ERR_INVALID_EMAIL;

	int email_count = register_count_email(u, email);
	if (email_count < 0)
	    return REGISTER_ERR_EMAILDB;
	if (email_count >= EMAILDB_LIMIT)
	    return REGISTER_ERR_TOO_MANY_ACCOUNTS;

	move(17, 0);
	outs(ANSI_COLOR(1;31)
		"\n提醒您: 如果之後發現您輸入的註冊資料有問題，不僅註冊會被取消，\n"
		"原本認證用的 E-mail 也不能再用來認證。\n" ANSI_RESET);
	char yn[3];
	getdata(16, 0, "請再次確認您輸入的 E-Mail 位置正確嗎? [y/N]",
		yn, sizeof(yn), LCECHO);
	clrtobot();
	if (yn[0] == 'y')
	    return REGISTER_OK;
    }
}

static int
register_count_email(const userec_t *u, const char *email)
{
    const char *userid = u ? u->userid : NULL;
    int count = 0;

#ifdef USE_EMAILDB
    {
	int r = emaildb_check_email(userid, email);
	if (r < 0)
	    return -1;
	if (count < r)
	    count = r;
    }
#endif

#ifdef USE_VERIFYDB
    {
	char lcemail[EMAILSZ];
	str_lower(lcemail, email);

	int count_self, count_other;
	if (verifydb_count_by_verify(VMETHOD_EMAIL, lcemail,
				     &count_self, &count_other) != 0)
	    return -1;
	if (count < count_other)
	    count = count_other;
    }
#endif

    return count;
}

static int
register_check_and_update_emaildb(const userec_t *u, const char *email)
{
    assert(u);

    int email_count = register_count_email(u, email);
    if (email_count < 0)
	return REGISTER_ERR_EMAILDB;
    if (email_count >= EMAILDB_LIMIT)
	return REGISTER_ERR_TOO_MANY_ACCOUNTS;

#ifdef USE_EMAILDB
    if (emaildb_update_email(cuser.userid, email) < 0)
	return REGISTER_ERR_EMAILDB;
#endif

#ifdef USE_VERIFYDB
    char lcemail[EMAILSZ];
    str_lower(lcemail, email);
    if (verifydb_set(u->userid, u->firstlogin, VMETHOD_EMAIL, lcemail, 0) != 0)
	return REGISTER_ERR_EMAILDB;
#endif

    return REGISTER_OK;
}

static void
toregister(char *email)
{
    clear();
    vs_hdr("認證設定");
    if (cuser.userlevel & PERM_NOREGCODE) {
	strcpy(email, "x");
	goto REGFORM2;
    }
    move(1, 0);
    outs("您好, 本站註冊認證的方式有:\n"
	 "  1.若您有 E-Mail  (本站不接受 yahoo, kimo等免費的 E-Mail)\n"
	 "    請輸入您的 E-Mail , 我們會寄發含有認證碼的信件給您\n"
	 "    收到後請到 (U)ser => (R)egister 輸入認證碼, 即可通過認證\n"
	 "\n"
	 "  2.若您沒有 E-Mail 或是一直無法收到認證信, 請輸入 x \n"
	 "  會有站長親自人工審核註冊資料，" ANSI_COLOR(1;33)
	   "但注意這可能會花上數天或更多時間。" ANSI_RESET "\n"
	 "**********************************************************\n"
	 "* 注意! 通常應該會在輸入完成後數分至數小時內收到認證信,  *\n"
	 "* 若過久未收到請到郵件垃圾桶檢查是否被當作垃圾信(SPAM)了,*\n"
	 "* 另外若輸入後發生認證碼錯誤請先確認輸入是否為最後一封   *\n"
	 "* 收到的認證信，若真的仍然不行請再重填一次 E-Mail.       *\n"
	 "**********************************************************\n");

    int err;
    do {
	err = register_email_input(&cuser, email);

	if (err == REGISTER_OK && strcasecmp(email, "x") != 0)
	    err = register_check_and_update_emaildb(&cuser, email);

	switch (err) {
	    case REGISTER_OK:
		break;

	    case REGISTER_ERR_INVALID_EMAIL:
		move(15, 0); clrtobot();
		move(17, 0);
		outs("指定的 E-Mail 不正確。可能你輸入的是免費的Email，\n");
		outs("或曾有使用者以本 E-Mail 認證後被取消資格。\n\n");
		outs("若您無 E-Mail 請輸入 x 由站長手動認證，\n");
		outs("但注意手動認證通常會花上數天以上的時間。\n");
		pressanykey();
		continue;

	    case REGISTER_ERR_EMAILDB:
		move(15, 0); clrtobot();
		move(17, 0);
		outs("email 認證系統發生問題, 請稍後再試，或輸入 x 採手動認證。\n");
		pressanykey();
		continue;

	    case REGISTER_ERR_TOO_MANY_ACCOUNTS:
		move(15, 0); clrtobot();
		move(17, 0);
		outs("指定的 E-Mail 已註冊過多帳號, 請使用其他 E-Mail, 或輸入 x 採手動認證\n");
		outs("但注意手動認證通常會花上數天以上的時間。\n");
		pressanykey();
		continue;

	    case REGISTER_ERR_CANCEL:
		vmsg("操作取消。");
		return;

	    default:
		assert(!"unhandled");
		exit(1);
		return;
	}
    } while (err != REGISTER_OK);

    if (strcasecmp(email, "x") != 0) {
	pwcuRegSetTemporaryJustify("<Email>", email);
	email_justify(cuser_ref);
	return;
    }

 REGFORM2:
    // Manual verification.
    if (!create_regform_request())
	vmsg("註冊申請單建立失敗。請至 " BN_BUGREPORT " 報告。");
}

int
u_register(void)
{
    char            rname[20], addr[50];
    char            career[40], email[EMAILSZ];
    char            inregcode[REGCODE_SZ], regcode[REGCODE_SZ];
    char            ans[3], *errcode;
    int		    i = 0;
    int             isForeign = (HasUserFlag(UF_FOREIGN)) ? 1 : 0;

#ifndef FOREIGN_REG
    isForeign = 0;
#endif

    if (cuser.userlevel & PERM_LOGINOK) {
	outs("您的身份確認已經完成，不需填寫申請表");
	return XEASY;
    }

    // TODO REGFORM 2 checks 2 parts.
    i = file_find_record(FN_REQLIST, cuser.userid);

    if (i > 0)
    {
	vs_hdr("註冊單尚在處理中");
	move(3, 0);
	prints("   您的註冊申請單尚在處理中(處理順位: %d)，請耐心等候\n\n", i);
	outs("   * 如果您之前曾使用 email 等認證方式通過註冊認證但又看到此訊息，\n"
	     "     代表您的認證由於資料不完整已被取消 (由於建立新看板的流程中\n"
	    "      有驗證板主註冊資料的程序，若您最近有申請開新看板中則屬此項)\n\n"
	     "   * 如果您已收到註冊碼卻看到這個畫面，代表您在使用 Email 註冊後\n"
	     "     " ANSI_COLOR(1;31) "又另外申請了站長直接人工審核的註冊申請單。"
		ANSI_RESET "\n"
	     "     進入人工審核程序後 Email 註冊碼自動失效，要等到審核完成\n"
	     "      (會多花很多時間，數天到數週是正常的) ，所以請耐心等候。\n\n");
	vmsg("您的註冊申請單尚在處理中");
	return FULLUPDATE;
    }

    strlcpy(rname, cuser.realname, sizeof(rname));
    strlcpy(addr,  cuser.address,  sizeof(addr));
    strlcpy(email, cuser.email,    sizeof(email));
    strlcpy(career,cuser.career,   sizeof(career));

    if (cuser.userlevel & PERM_NOREGCODE) {
	vmsg("您不被允許使用認證碼認證。請填寫註冊申請單");
	goto REGFORM;
    }

    // getregcode(regcode);

    if (cuser.email[0] && /* 已經第一次填過了~ ^^" */
	strcmp(cuser.email, "x") != 0 &&	/* 上次手動認證失敗 */
	strcmp(cuser.email, "X") != 0)
    {
	vs_hdr("EMail認證");
	move(2, 0);

	prints("請輸入您的認證碼。(由 %s 開頭無空白的十三碼)\n"
               "若尚未收到信件或不想現在輸入可直接按 ENTER 離開，\n"
	       "或輸入 x 來重新填寫 E-Mail 或改由站長手動認證\n",
               REGCODE_INITIAL);
	inregcode[0] = 0;

	do{
	    getdata(10, 0, "您的認證碼：",
		    inregcode, sizeof(inregcode), DOECHO);
	    if( ! *inregcode ||
                strcmp(inregcode, "x") == 0 ||
                strcmp(inregcode, "X") == 0 )
		break;
	    if( strlen(inregcode) != REGCODE_LEN || inregcode[0] == ' ') {
                LOG_IF((LOG_CONF_BAD_REG_CODE && inregcode[0]),
                       log_filef("log/reg_badcode.log", LOG_CREAT,
                                 "%s %s INCOMPLETE [%s]\n",
                                 Cdate(&now), cuser.userid, inregcode));
		vmsg("認證碼輸入不完整，總共應有十三碼，沒有空白字元。");
            } else if(inregcode[0] != REGCODE_INITIAL[0] ||
                      inregcode[1] != REGCODE_INITIAL[1] ) {
		/* old regcode */
                LOG_IF(LOG_CONF_BAD_REG_CODE,
                       log_filef("log/reg_badcode.log", LOG_CREAT,
                                 "%s %s INVALID [%s]\n",
                                 Cdate(&now), cuser.userid, inregcode));
		vmsg("輸入的認證碼錯誤，" // "或因系統昇級已失效，"
		     "請輸入 x 重填一次 E-Mail");
	    }
	    else
		break;
	} while( 1 );

	if (!*inregcode) {
            // simply enter.
            return FULLUPDATE;
	} else if (getregcode(regcode) == 0 &&
		   /* make it case insensitive. */
		   strcasecmp(inregcode, regcode) == 0) {
	    int  unum;
	    char justify[sizeof(cuser.justify)] = "";
	    delregcodefile();
	    if ((unum = searchuser(cuser.userid, NULL)) == 0) {
		vmsg("系統錯誤，查無此人！");
		u_exit("getuser error");
		exit(0);
	    }
	    mail_muser(cuser, "[註冊成功囉]", "etc/registeredmail");
#if FOREIGN_REG_DAY > 0
	    if(HasUserFlag(UF_FOREIGN))
		mail_muser(cuser, "[出入境管理局]", "etc/foreign_welcome");
#endif
	    snprintf(justify, sizeof(justify), "<E-Mail>: %s", Cdate(&now));
	    pwcuRegCompleteJustify(justify);
	    outs("\n註冊成功, 重新上站後將取得完整權限\n"
		   "請按下任一鍵跳離後重新上站~ :)");
	    pressanykey();
	    u_exit("registed");
	    exit(0);
	    // XXX shall never reach here.
	    return QUIT;

	} else if (strcasecmp(inregcode, "x") != 0) {
	    if (regcode[0])
	    {
		vmsg("認證碼錯誤！");
                LOG_IF(LOG_CONF_BAD_REG_CODE,
                       log_filef("log/reg_badcode.log", LOG_CREAT,
                                 "%s %s INCORRECT [%s] (should be: %s)\n",
                                 Cdate(&now), cuser.userid, inregcode,
                                 regcode));
		return FULLUPDATE;
	    }
	    else
	    {
		vmsg("認證碼已過期，請重新註冊。");
		toregister(email);
		return FULLUPDATE;
	    }
	} else {
            char fpath[PATHLEN];
            time4_t  last_request_time;
            int hours = 0;

            getregfile(fpath);
            last_request_time = dasht(fpath);
            if (last_request_time < now &&
                last_request_time + DAY_SECONDS / 2 > now)
                hours = (last_request_time + DAY_SECONDS / 2 - now) / 3600 + 1;
            if (hours > 0) {
                outs("由於某些使用者的信箱收信間隔較長、"
                     "且每次寄出新認證信時前封認證碼會自動失效，\n"
                     // 為了避免有人搞不清狀況跑去 SYSOP 哭哭說認證碼無效，
                     "每次重寄認證信或變更 EMail 要間隔 12 小時。\n");
                prints("距離您下次可以重新認證尚有 %d 小時。", hours);
                pressanykey();
                return FULLUPDATE;
            }
	    toregister(email);
	    return FULLUPDATE;
	}
    }

    REGFORM:
    getdata(b_lines - 1, 0, "您確定要填寫註冊單嗎(Y/N)？[N] ",
	    ans, 3, LCECHO);
    if (ans[0] != 'y')
	return FULLUPDATE;

    // show REGNOTES_ROOT front page
    if (dashs(REGNOTES_ROOT "front") > 0)
    {
	clear();
	vs_hdr("註冊單填寫說明");
	show_file(REGNOTES_ROOT "front", 1, t_lines-2, SHOWFILE_ALLOW_ALL);
	vmsg(NULL);
    }

    move(2, 0);
    clrtobot();

    while (1) {
	clear();
	move(1, 0);
	prints("%s(%s) 您好，請據實填寫以下的資料:",
	       cuser.userid, cuser.nickname);
#ifdef FOREIGN_REG
	{
	    char not_fore[2] = "";  // use default values instead of pre-inputs

	    getfield(2, isForeign ? "y/N" : "Y/n", REGNOTES_ROOT "foreign",  "是否現在住在台灣", not_fore, 2);

            // XXX NOTE: the question we ask was "Aren't you a foreigner" in
            // Chinese, so the answer must be opposite to isForeign.
	    if (isForeign)
	    {
		// default n
		isForeign = (not_fore[0] == 'y' || not_fore[0] == 'Y') ? 0 : 1;
	    } else {
		// default y
		isForeign = (not_fore[0] == 'n' || not_fore[0] == 'N') ? 1 : 0;
	    }
	}
	move(2, 0); prints("  是否現在住在台灣: %s\n", isForeign ? "N (否)" : "Y (是)");
#endif
	while (1) {
	    getfield(4,
#ifdef FOREIGN_REG
                     "請用本名",
#else
                     "請用中文",
#endif
		     REGNOTES_ROOT "name",
                     "真實姓名", rname, 20);
	    if( (errcode = isvalidname(rname)) == NULL )
		break;
	    else
		vmsg(errcode);
	}

	while (1) {
	    getfield(5, "(畢業)學校(含" ANSI_COLOR(1;33) "系所年級" ANSI_RESET ")或單位職稱",
		    REGNOTES_ROOT "career", "服務單位", career, 40);
	    if ((errcode = isvalidcareer(career)) == NULL)
		break;
	    else
		vmsg(errcode);
	}

	while (1) {
	    getfield(6, "含" ANSI_COLOR(1;33) "縣市" ANSI_RESET "及門寢號碼"
		     "(台北請加" ANSI_COLOR(1;33) "行政區" ANSI_RESET ")",
		     REGNOTES_ROOT "address", "目前住址", addr, sizeof(addr));
	    if ((errcode = isvalidaddr(addr, isForeign)) == NULL)
		break;
	    else
		vmsg(errcode);
	}

	getdata(20, 0, "以上資料是否正確(Y/N)？(Q)取消註冊 [N] ",
		ans, 3, LCECHO);
	if (ans[0] == 'q')
	    return 0;
	if (ans[0] == 'y')
	    break;
    }

    // copy values to cuser
    pwcuRegisterSetInfo(rname, addr, career, email, isForeign);

    // if reach here, email is apparently 'x'.
    toregister(email);

    return FULLUPDATE;
}

////////////////////////////////////////////////////////////////////////////
// Regform Utilities
////////////////////////////////////////////////////////////////////////////

// TODO define and use structure instead, even in reg request file.
typedef struct {
    userec_t	u;	// user record
    char	online;

} RegformEntry;

// regform format utilities

static int
print_regform_entry(const RegformEntry *pre, FILE *fp, int close)
{
    fprintf(fp, "uid: %s\n",	pre->u.userid);
    fprintf(fp, "name: %s\n",	pre->u.realname);
    fprintf(fp, "career: %s\n", pre->u.career);
    fprintf(fp, "addr: %s\n",	pre->u.address);
    fprintf(fp, "lasthost: %s\n", pre->u.lasthost);
    if (close)
	fprintf(fp, "----\n");
    return 1;
}

// The size to hold concat_regform_entry_localized
#define REGFORM_LOCALIZED_ENTRIES_BUFSIZE   (10 * STRLEN)

static int
concat_regform_entry_localized(const RegformEntry *pre, char *result, int maxlen)
{
    int len = strlen(result);
    snprintf(result + len, maxlen - len, "使用者ID: %s\n", pre->u.userid);
    len = strlen(result);
    snprintf(result + len, maxlen - len, "真實姓名: %s\n", pre->u.realname);
    len = strlen(result);
    snprintf(result + len, maxlen - len, "職業學校: %s\n", pre->u.career);
    len = strlen(result);
    snprintf(result + len, maxlen - len, "目前住址: %s\n", pre->u.address);
    len = strlen(result);
    snprintf(result + len, maxlen - len, "上站位置: %s\n", pre->u.lasthost);
    len = strlen(result);
    snprintf(result + len, maxlen - len, "----\n");
    return 1;
}

static int
print_regform_entry_localized(const RegformEntry *pre, FILE *fp)
{
    char buf[REGFORM_LOCALIZED_ENTRIES_BUFSIZE];
    buf[0] = '\0';
    concat_regform_entry_localized(pre, buf, sizeof(buf));
    fputs(buf, fp);
    return 1;
}

int
append_regform(const RegformEntry *pre, const char *logfn, const char *ext)
{
    FILE *fout = fopen(logfn, "at");
    if (!fout)
	return 0;

    print_regform_entry(pre, fout, 0);
    if (ext)
    {
	syncnow();
	fprintf(fout, "Date: %s\n", Cdate(&now));
	if (*ext)
	    fputs(ext, fout);
    }
    // close it
    fprintf(fout, "----\n");
    fclose(fout);
    return 1;
}

// prototype declare
static void regform_print_reasons(const char *reason, FILE *fp);
static void regform_concat_reasons(const char *reason, char *result, int maxlen);

int regform_estimate_queuesize()
{
    return dashs(FN_REQLIST) / IDLEN;
}

/////////////////////////////////////////////////////////////////////////////
// Administration (SYSOP Validation)
/////////////////////////////////////////////////////////////////////////////

#define REJECT_REASONS	(5)
#define REASON_LEN	(60)
static const char *reasonstr[REJECT_REASONS] = {
    "請輸入真實姓名",
    "請詳填最高學歷(含入學年度)或服務單位(含單位所在縣市及職稱)",
    "請詳填住址(含鄉鎮市區名, 並請填寫至最小單位)",
    "請詳實填寫註冊申請表, 填寫說明請見: http://goo.gl/pyUejf",
    "請用中文填寫申請單",
};

#define REASON_FIRSTABBREV '0'
#define REASON_IN_ABBREV(x) \
    ((x) >= REASON_FIRSTABBREV && (x) - REASON_FIRSTABBREV < REJECT_REASONS)
#define REASON_EXPANDABBREV(x)	 reasonstr[(x) - REASON_FIRSTABBREV]

void
regform_log2board(const RegformEntry *pre, char accepted,
	const char *reason, int priority)
{
#ifdef BN_ID_RECORD
    char title[STRLEN];
    char *title2 = NULL;

    // The message may contain ANSI escape sequences (regform_concat_reasons)
    char msg[ANSILINELEN * REJECT_REASONS + REGFORM_LOCALIZED_ENTRIES_BUFSIZE];

    snprintf(title, sizeof(title),
	    "[審核] %s: %s (%s: %s)",
	    accepted ? "○通過":"╳退回", pre->u.userid,
	    priority ? "指定審核" : "審核者",
	    cuser.userid);

    // reduce mail header title
    title2 = strchr(title, ' ');
    if (title2) title2++;


    // construct msg
    strlcpy(msg, title2 ? title2 : title, sizeof(msg));
    strlcat(msg, "\n", sizeof(msg));
    if (!accepted) {
	regform_concat_reasons(reason, msg, sizeof(msg));
    }
    strlcat(msg, "\n", sizeof(msg));
    concat_regform_entry_localized(pre, msg, sizeof(msg));

    post_msg(BN_ID_RECORD, title, msg, "[註冊系統]");
#endif  // BN_ID_RECORD
}

void
regform_log2file(const RegformEntry *pre, char accepted,
	const char *reason, int priority)
{
#ifdef FN_ID_RECORD
    // The message may contain ANSI escape sequences (regform_concat_reasons)
    char msg[ANSILINELEN * REJECT_REASONS + REGFORM_LOCALIZED_ENTRIES_BUFSIZE];

    snprintf(msg, sizeof(msg),
	    "%s\n%s: %s (%s: %s)\n",
            Cdate(&now),
	    accepted ? "○通過":"╳退回", pre->u.userid,
	    priority ? "指定審核" : "審核者",
	    cuser.userid);

    // construct msg
    if (!accepted) {
	regform_concat_reasons(reason, msg, sizeof(msg));
    }
    strlcat(msg, "\n", sizeof(msg));
    concat_regform_entry_localized(pre, msg, sizeof(msg));
    log_file(FN_ID_RECORD, LOG_CREAT, msg);
#else
    (void)pre;
    (void)accepted;
    (void)reason;
    (void)priority;
#endif  // FN_ID_RECORD
}

void
regform_accept(const char *userid, const char *justify)
{
    char buf[PATHLEN];
    int unum = 0;
    userec_t muser;

    unum = getuser(userid, &muser);
    if (unum == 0)
	return; // invalid user

    muser.userlevel |= (PERM_LOGINOK | PERM_POST);
    strlcpy(muser.justify, justify, sizeof(muser.justify));
    // manual accept sets email to 'x'
    strlcpy(muser.email, "x", sizeof(muser.email));

    // handle files
    sethomefile(buf, muser.userid, FN_REJECT_NOTIFY);
    unlink(buf);

    // update password file
    passwd_sync_update(unum, &muser);

    // alert online users?
    if (search_ulist(unum))
    {
	kick_all(muser.userid);
    }

    // According to suggestions by PTT BBS account sysops,
    // it is better to use anonymous here.
#if FOREIGN_REG_DAY > 0
    if(muser.uflag & UF_FOREIGN)
	mail_log2id(muser.userid, "[System] Registration Complete ", "etc/foreign_welcome",
		"[SYSTEM]", 1, 0);
    else
#endif
    // last: send notification mail
    mail_log2id(muser.userid, "[系統通知] 註冊成功 ", "etc/registered",
	    "[系統通知]", 1, 0);
}

void
regform_reject(const char *userid, const char *reason, const RegformEntry *pre)
{
    char buf[PATHLEN];
    FILE *fp = NULL;
    int unum = 0;
    userec_t muser;

    unum = getuser(userid, &muser);
    if (unum == 0)
	return; // invalid user

    muser.userlevel &= ~(PERM_LOGINOK | PERM_POST);

    // handle files

    // update password file
    passwd_sync_update(unum, &muser);

    // alert online users?
    if (search_ulist(unum))
    {
	sendalert(muser.userid,  ALERT_PWD_PERM); // force to reload perm
	kick_all(muser.userid);
    }

    // last: send notification
    mkuserdir(muser.userid);
    sethomefile(buf, muser.userid, FN_REJECT_NOTIFY);
    fp = fopen(buf, "wt");
    assert(fp);
    syncnow();

    // log reference for mail-reply.
    fprintf(fp, "#%010d\n\n", usernum);

    if(pre) print_regform_entry_localized(pre, fp);
    fprintf(fp, "%s 註冊失敗。\n", Cdate(&now));

    // multiple abbrev loop
    regform_print_reasons(reason, fp);
    fprintf(fp, "--\n");
    fclose(fp);

    // if current site has extra notes
    if (dashf(FN_REJECT_NOTES))
	AppendTail(FN_REJECT_NOTES, buf, 0);

    // According to suggestions by PTT BBS account sysops,
    // it is better to use anonymous here.
    //
    // XXX how to handle the notification file better?
    // mail_log2id: do not use move.
    // mail_muser(muser, "[註冊失敗]", buf);

    // use regform2! no need to set 'newmail'.
    mail_log2id(muser.userid, "[註冊失敗記錄]", buf, "[註冊系統]", 0, 0);
}

// New Regform UI
static void
prompt_regform_ui()
{
    vs_footer(" 審核 ",
	    " (y)接受(n)拒絕(d)丟掉 (s)跳過(u)復原 (空白/PgDn)儲存+下頁 (q/END)結束");
}

static void
regform_concat_reasons(const char *reason, char *result, int maxlen)
{
    int len = strlen(result);
    // multiple abbrev loop
    if (REASON_IN_ABBREV(reason[0]))
    {
	int i = 0;
	for (i = 0; reason[i] && REASON_IN_ABBREV(reason[i]); i++) {
            snprintf(result + len, maxlen - len,
                     ANSI_COLOR(1;33)
                     "[退回原因] %s" ANSI_RESET "\n",
                     REASON_EXPANDABBREV(reason[i]));
            len = strlen(result);
	}
    } else {
        snprintf(result + len, maxlen - len,
                 ANSI_COLOR(1;33) "[退回原因] %s" ANSI_RESET "\n", reason);
    }
}

static void
regform_print_reasons(const char *reason, FILE *fp)
{
    // the messages may contain ANSI escapes
    char msg[ANSILINELEN * REJECT_REASONS];
    msg[0] = '\0';
    regform_concat_reasons(reason, msg, sizeof(msg));
    fputs(msg, fp);
}

static void
resolve_reason(char *s, int y, int force)
{
    // should start with REASON_FIRSTABBREV
    const char *reason_prompt =
	" (0)真實姓名 (1)詳填系級 (2)完整住址"
	" (3)確實填寫 (4)中文填寫";

    s[0] = 0;
    move(y, 0);
    outs(reason_prompt); outs("\n");

    do {
	getdata(y+1, 0, "退回原因: ", s, REASON_LEN, DOECHO);

	// convert abbrev reasons (format: single digit, or multiple digites)
	if (REASON_IN_ABBREV(s[0]))
	{
	    if (s[1] == 0) // simple replace ment
	    {
		strlcpy(s, REASON_EXPANDABBREV(s[0]),
			REASON_LEN);
	    } else {
		// strip until all digites
		char *p = s;
		while (*p)
		{
		    if (!REASON_IN_ABBREV(*p))
			*p = ' ';
		    p++;
		}
		strip_blank(s, s);
		strlcat(s, " [多重原因]", REASON_LEN);
	    }
	}

	if (!force && !*s)
	    return;

	if (strlen(s) < 4)
	{
	    if (vmsg("原因太短。 要取消退回嗎？ (y/N): ") == 'y')
	    {
		*s = 0;
		return;
	    }
	}
    } while (strlen(s) < 4);
}

////////////////////////////////////////////////////////////////////////////
// Regform2 API
////////////////////////////////////////////////////////////////////////////

// registration queue
int
regq_append(const char *userid)
{
    if (file_append_record(FN_REQLIST, userid) < 0)
	return 0;
    return 1;
}

int
regq_find(const char *userid)
{
    return file_find_record(FN_REQLIST, userid);
}

int
regq_delete(const char *userid)
{
    return file_delete_record(FN_REQLIST, userid, 0);
}

// user home regform operation
int
regfrm_exist(const char *userid)
{
    char fn[PATHLEN];
    sethomefile(fn, userid, FN_REGFORM);
    return  dashf(fn) ? 1 : 0;
}

int
regfrm_delete(const char *userid)
{
    char fn[PATHLEN];
    sethomefile(fn, userid, FN_REGFORM);

#ifdef DBG_DRYRUN
    // dry run!
    vmsgf("regfrm_delete (%s)", userid);
    return 1;
#endif

    // directly delete.
    unlink(fn);

    // remove from queue
    regq_delete(userid);
    return 1;
}

int
regfrm_load(const char *userid, RegformEntry *pre)
{
    // FILE *fp = NULL;
    char fn[PATHLEN];
    int unum = 0;

    memset(pre, 0, sizeof(RegformEntry));

    // first check if user exists.
    unum = getuser(userid, &(pre->u));

    // valid unum starts at 1.
    if (unum < 1)
	return 0;

    // check if regform exists.
    sethomefile(fn, userid, FN_REGFORM);
    if (!dashf(fn))
	return 0;

#ifndef DBG_DRYRUN
    // check if user is already registered
    if (pre->u.userlevel & PERM_LOGINOK)
    {
	regfrm_delete(userid);
	return 0;
    }
#endif

    // load regform
    // (deprecated in current version, we use real user data now)

    // fill RegformEntry data
    pre->online = search_ulist(unum) ? 1 : 0;

    return 1;
}

int
regfrm_save(const char *userid, const RegformEntry *pre)
{
    FILE *fp = NULL;
    char fn[PATHLEN];
    int ret = 0;
    sethomefile(fn, userid, FN_REGFORM);

    fp = fopen(fn, "wt");
    if (!fp)
	return 0;
    ret = print_regform_entry(pre, fp, 1);
    fclose(fp);
    return ret;
}

int
regfrm_trylock(const char *userid)
{
    int fd = 0;
    char fn[PATHLEN];
    sethomefile(fn, userid, FN_REGFORM);
    if (!dashf(fn)) return 0;
    fd = open(fn, O_RDONLY);
    if (fd < 0) return 0;
    if (flock(fd, LOCK_EX|LOCK_NB) == 0)
	return fd;
    close(fd);
    return 0;
}

int
regfrm_unlock(int lockfd)
{
    int fd = lockfd;
    if (lockfd <= 0)
	return 0;
    lockfd =  flock(fd, LOCK_UN) == 0 ? 1 : 0;
    close(fd);
    return lockfd;
}

// regform processors
int
regfrm_accept(RegformEntry *pre, int priority)
{
    char justify[REGLEN+1], buf[STRLEN*2];
    char fn[PATHLEN], fnlog[PATHLEN];

#ifdef DBG_DRYRUN
    // dry run!
    vmsg("regfrm_accept");
    return 1;
#endif

    sethomefile(fn, pre->u.userid, FN_REGFORM);

    // build justify string
    snprintf(justify, sizeof(justify),
	    "[%s] %s", cuser.userid, Cdate(&now));

    // call handler
    regform_accept(pre->u.userid, justify);

    // log to user home
    sethomefile(fnlog, pre->u.userid, FN_REGFORM_LOG);
    append_regform(pre, fnlog, "");

    // log to global history
    snprintf(buf, sizeof(buf), "Approved: %s -> %s\n",
	    cuser.userid, pre->u.userid);
    append_regform(pre, FN_REGISTER_LOG, buf);

    // log to file / board
    regform_log2file(pre, 1, NULL, priority);
    regform_log2board(pre, 1, NULL, priority);

    // remove from queue
    unlink(fn);
    regq_delete(pre->u.userid);
    return 1;
}

int
regfrm_reject(RegformEntry *pre, const char *reason, int priority)
{
    char buf[STRLEN*2];
    char fn[PATHLEN];

#ifdef DBG_DRYRUN
    // dry run!
    vmsg("regfrm_reject");
    return 1;
#endif

    sethomefile(fn, pre->u.userid, FN_REGFORM);

    // call handler
    regform_reject(pre->u.userid, reason, pre);

    // log to global history
    snprintf(buf, sizeof(buf), "Rejected: %s -> %s [%s]\n",
	    cuser.userid, pre->u.userid, reason);
    append_regform(pre, FN_REGISTER_LOG, buf);

    // log to board
    regform_log2board(pre, 0, reason, priority);

    // remove from queue
    unlink(fn);
    regq_delete(pre->u.userid);
    return 1;
}

// working queue
FILE *
regq_init_pull()
{
    FILE *fp = tmpfile(), *src =NULL;
    char buf[STRLEN];
    if (!fp) return NULL;
    src = fopen(FN_REQLIST, "rt");
    if (!src) { fclose(fp); return NULL; }
    while (fgets(buf, sizeof(buf), src))
	fputs(buf, fp);
    fclose(src);
    rewind(fp);
    return fp;
}

int
regq_pull(FILE *fp, char *uid)
{
    char buf[STRLEN];
    size_t idlen = 0;
    uid[0] = 0;
    if (fgets(buf, sizeof(buf), fp) == NULL)
	return 0;
    idlen = strcspn(buf, str_space);
    if (idlen < 1) return 0;
    if (idlen > IDLEN) idlen = IDLEN;
    strlcpy(uid, buf, idlen+1);
    return 1;
}

int
regq_end_pull(FILE *fp)
{
    // no need to unlink because fp is a tmpfile.
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

// UI part
int
ui_display_regform_single(
	const RegformEntry *pre,
	int tid, char *reason)
{
    int c;
    const userec_t *xuser = &(pre->u);

    while (1)
    {
	move(1, 0);
	user_display(xuser, 1);
	move(14, 0);
	prints(ANSI_COLOR(1;32)
		"--------------- 這是第 %2d 份註冊單 -----------------------"
		ANSI_RESET "\n", tid);
	prints("  %-12s: %s %s\n",	"帳號", pre->u.userid,
		(xuser->userlevel & PERM_NOREGCODE) ?
		ANSI_COLOR(1;31) "  [T:禁止使用認證碼註冊]" ANSI_RESET:
		"");
	prints("0.%-12s: %s%s\n",	"真實姓名", pre->u.realname,
		xuser->uflag & UF_FOREIGN ? " (外籍)" :
		"");
	prints("1.%-12s: %s\n",	"服務單位", pre->u.career);
	prints("2.%-12s: %s\n",	"目前住址", pre->u.address);

	move(b_lines, 0);
	outs("是否接受此資料(Y/N/Q/Del/Skip)？[S] ");

	// round to ASCII
	while ((c = vkey()) > 0xFF);
	c = tolower(c);

	if (c == 'y' || c == 'q' || c == 'd' || c == 's')
	    return c;
	if (c == 'n')
	{
	    int n = 0;
	    move(3, 0);
	    outs("\n" ANSI_COLOR(1;31)
		    "請提出退回申請表原因，按 <Enter> 取消:\n" ANSI_RESET);
	    for (n = 0; n < REJECT_REASONS; n++)
		prints("%d) %s\n", n, reasonstr[n]);
	    outs("\n\n\n"); // preserved for prompt

	    getdata(3+2+REJECT_REASONS+1, 0,"退回原因: ",
		    reason, REASON_LEN, DOECHO);
	    if (reason[0] == 0)
		continue;
	    // interprete reason
	    return 'n';
	}
	else if (REASON_IN_ABBREV(c))
	{
	    // quick set
	    sprintf(reason, "%c", c);
	    return 'n';
	}
	return 's';
    }
    // shall never reach here
    return 's';
}

void
regform2_validate_single(const char *xuid)
{
    int lfd = 0;
    int tid = 0;
    char uid[IDLEN+1];
    char rsn[REASON_LEN];
    FILE *fpregq = regq_init_pull();
    RegformEntry re;

    if (xuid && !*xuid)
	xuid = NULL;

    if (!fpregq)
	return;

    while (regq_pull(fpregq, uid))
    {
	int abort = 0;

	// if target assigned, loop until given target.
	if (xuid && strcasecmp(uid, xuid) != 0)
	    continue;

	// try to load regform.
	if (!regfrm_load(uid, &re))
	{
	    regq_delete(uid);
	    continue;
	}

	// try to lock
	lfd = regfrm_trylock(uid);
	if (lfd <= 0)
	    continue;

	tid ++;

	// display regform and process
	switch(ui_display_regform_single(&re, tid, rsn))
	{
	    case 'y': // accept
		regfrm_accept(&re, xuid ? 1 : 0);
		break;

	    case 'd': // delete
		regfrm_delete(uid);
		break;

	    case 'q': // quit
		abort = 1;
		break;

	    case 'n': // reject
		regfrm_reject(&re, rsn, xuid ? 1 : 0);
		break;

	    case 's': // skip
		// do nothing.
		break;

	    default: // shall never reach here
		assert(0);
		break;
	}

	// final processing
	regfrm_unlock(lfd);

	if (abort)
	    break;
    }
    regq_end_pull(fpregq);
    LOG_IF(LOG_CONF_VALIDATE_REG,
           log_filef("log/validate_reg.log", LOG_CREAT,
                     "%s %s SINGLE finished: %d forms\n",
                     Cdatelite(&now), cuser.userid, tid));
    // finishing
    clear(); move(5, 0);
    if (xuid && tid == 0)
	prints("未發現 %s 的註冊單。", xuid);
    else
	prints("您檢視了 %d 份註冊單。", tid);
    pressanykey();
}

// According to the (soft) max terminal size definition.
#define MAX_FORMS_IN_PAGE (100)

int
regform2_validate_page(int dryrun)
{
    RegformEntry forms [MAX_FORMS_IN_PAGE];
    char ans	[MAX_FORMS_IN_PAGE];
    int  lfds	[MAX_FORMS_IN_PAGE];
    char rejects[MAX_FORMS_IN_PAGE][REASON_LEN];	// reject reason length
    char rsn	[REASON_LEN];
    int cforms = 0,	// current loaded forms
	ci = 0, // cursor index
	ch = 0,	// input key
	i;
    int tid = 0;
    char uid[IDLEN+1];
    FILE *fpregq = regq_init_pull();
    int yMsg = 0;
    int forms_in_page = (t_lines - 3) / 2;

    if (!fpregq)
	return 0;

    if (forms_in_page >= MAX_FORMS_IN_PAGE)
        forms_in_page = MAX_FORMS_IN_PAGE -1;

    yMsg = forms_in_page * 2 + 1;
    while (ch != 'q')
    {
	// initialize and prepare
	memset(ans,	0, sizeof(ans));
	memset(rejects, 0, sizeof(rejects));
	memset(forms,	0, sizeof(forms));
	memset(lfds,	0, sizeof(lfds));
	cforms = 0;
	clear();

	// load forms
	while (cforms < forms_in_page)
	{
	    if (!regq_pull(fpregq, uid))
		break;
	    i = cforms; // align index

	    // check if user exists.
	    if (!regfrm_load(uid, &forms[i]))
	    {
		regq_delete(uid);
		continue;
	    }

	    // try to lock
            lfds[i] = regfrm_trylock(uid);
            if (lfds[i] <= 0)
                continue;

	    // assign default answers
	    if (forms[i].u.userlevel & PERM_LOGINOK)
		ans[i] = 'd';
#ifdef REGFORM_DISABLE_ONLINE_USER
	    else if (forms[i].online)
		ans[i] = 's';
#endif // REGFORM_DISABLE_ONLINE_USER

	    // display regform
	    move(i*2, 0);
	    prints("  %2d%s %s%-12s " ANSI_RESET,
		    i+1,
		    ( (forms[i].u.userlevel & PERM_LOGINOK) ?
		      ANSI_COLOR(1;33) "Y" :
#ifdef REGFORM_DISABLE_ONLINE_USER
			  forms[i].online ? "s" :
#endif
			  "."),
		    forms[i].online ?  ANSI_COLOR(1;35) : ANSI_COLOR(1),
		    forms[i].u.userid);

	    prints( ANSI_COLOR(1;31) "%19s "
		    ANSI_COLOR(1;32) "%-40s" ANSI_RESET"\n",
		    forms[i].u.realname, forms[i].u.career);

	    move(i*2+1, 0);
	    prints("    %s ", (forms[i].u.userlevel & PERM_NOREGCODE) ?
                              ANSI_COLOR(1;31) "T" ANSI_RESET : " ");
            prints("%-50s" ANSI_COLOR(0;33) "%s" ANSI_RESET "\n",
                   forms[i].u.address, forms[i].u.lasthost);

	    cforms++, tid ++;
	}

	// if no more forms then leave.
	if (cforms < 1)
	    break;

	// adjust cursor if required
	if (ci >= cforms)
	    ci = cforms-1;

	// display page info
	vbarf(ANSI_REVERSE "\t%s 已顯示 %d 份註冊單 ", // "(%2d%%)  ",
		    dryrun? "(測試模式)" : "",
		    tid);

	// handle user input
	prompt_regform_ui();
	ch = 0;
	while (ch != 'q' && ch != ' ') {
	    ch = cursor_key(ci*2, 0);
	    switch (ch)
	    {
		// nav keys
		case KEY_UP:
		case 'k':
		    if (ci > 0) ci--;
		    break;

		case KEY_DOWN:
		case 'j':
		    ch = 'j'; // go next
		    break;

		    // quick nav (assuming to FORMS_IN_PAGE=10)
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
		    ci = ch - '1';
		    if (ci >= cforms) ci = cforms-1;
		    break;
		case '0':
		    ci = 10-1;
		    if (ci >= cforms) ci = cforms-1;
		    break;

		case KEY_HOME: ci = 0; break;
		    /*
		case KEY_END:  ci = cforms-1; break;
		    */

		    // abort
		case KEY_END:
		case 'q':
		    ch = 'q';
		    if (vans("確定要離開了嗎？ (本頁變更將不會儲存) [y/N]: ") != 'y')
		    {
			prompt_regform_ui();
			ch = 0;
			continue;
		    }
		    break;

		    // prepare to go next page
		case KEY_PGDN:
		case ' ':
		    ch = ' ';

		    {
			int blanks = 0;
			// solving blank (undecided entries)
			for (i = 0, blanks = 0; i < cforms; i++)
			    if (ans[i] == 0) blanks ++;

			if (!blanks)
			    break;

			// have more blanks
			ch = vansf("尚未指定的 %d 個項目要: (S跳過/y通過/n拒絕/e繼續編輯): ",
				blanks);
		    }

		    if (ch == 'e')
		    {
			prompt_regform_ui();
			ch = 0;
			continue;
		    }
		    if (ch == 'y') {
			// do nothing.
		    } else if (ch == 'n') {
			// query reject reason
			resolve_reason(rsn, yMsg, 1);
			if (*rsn == 0)
			    ch = 's';
		    } else ch = 's';

		    // filling answers
		    for (i = 0; i < cforms; i++)
		    {
			if (ans[i] != 0)
			    continue;
			ans[i] = ch;
			if (ch != 'n')
			    continue;
			strlcpy(rejects[i], rsn, REASON_LEN);
		    }

		    ch = ' '; // go to page mode!
		    break;

		    // function keys
		case 'y':	// accept
#ifdef REGFORM_DISABLE_ONLINE_USER
		    if (forms[ci].online)
		    {
			vmsg("暫不開放審核在線上使用者。");
			break;
		    }
#endif
		case 's':	// skip
		case 'd':	// delete
		case Ctrl('D'): // delete
		case KEY_DEL:	// delete
		    if (ch == KEY_DEL || ch == Ctrl('D')) ch = 'd';

		    grayout(ci*2, ci*2+1, GRAYOUT_DARK);
		    move_ansi(ci*2, 4); outc(ch);
		    ans[ci] = ch;
		    ch = 'j'; // go next
		    break;

		case 'u':	// undo
#ifdef REGFORM_DISABLE_ONLINE_USER
		    if (forms[ci].online)
		    {
			vmsg("暫不開放審核在線上使用者。");
			break;
		    }
#endif
		    grayout(ci*2, ci*2+1, GRAYOUT_NORM);
		    move_ansi(ci*2, 4); outc('.');
		    ans[ci] = 0;
		    ch = 'j'; // go next
		    break;

		case 'n':	// reject
#ifdef REGFORM_DISABLE_ONLINE_USER
		    if (forms[ci].online)
		    {
			vmsg("暫不開放審核在線上使用者。");
			break;
		    }
#endif
		    // query for reason
		    resolve_reason(rejects[ci], yMsg, 0);
		    move(yMsg, 0); clrtobot();
		    prompt_regform_ui();

		    if (!rejects[ci][0])
			break;

		    move(yMsg, 0);
		    prints("退回 %s 註冊單原因:\n %s\n",
			    forms[ci].u.userid, rejects[ci]);

		    // do reject
		    grayout(ci*2, ci*2+1, GRAYOUT_DARK);
		    move_ansi(ci*2, 4); outc(ch);
		    ans[ci] = ch;
		    ch = 'j'; // go next

		    break;
	    } // switch(ch)

	    // change cursor
	    if (ch == 'j' && ++ci >= cforms)
		ci = cforms -1;
	} // while(ch != QUIT/SAVE)

	// if exit, we still need to skip all read forms
	if (ch == 'q')
	{
	    for (i = 0; i < cforms; i++)
		ans[i] = 's';
	}

	// page complete (save).
	assert(ch == ' ' || ch == 'q');

	// save/commit if required.
	if (dryrun)
	{
	    // prmopt for debug
	    clear();
	    vs_hdr("測試模式");
	    outs("您正在執行測試模式，所以剛審的註冊單並不會生效。\n"
		    "下面列出的是剛才您審完的結果:\n\n");

	    for (i = 0; i < cforms; i++)
	    {
		char justify[REGLEN+1];
		if (ans[i] == 'y')
		    snprintf(justify, sizeof(justify), // build justify string
			    "%s %s", cuser.userid, Cdate(&now));

		prints("%2d. %-12s - %c %s\n", i+1, forms[i].u.userid, ans[i],
			ans[i] == 'n' ? rejects[i] :
			ans[i] == 'y' ? justify : "");
	    }
	    if (ch != 'q')
		pressanykey();
	}
	else
	{
	    // real functionality
	    for (i = 0; i < cforms; i++)
	    {
		switch(ans[i])
		{
		    case 'y': // accept
			regfrm_accept(&forms[i], 0);
			break;

		    case 'd': // delete
			regfrm_delete(forms[i].u.userid);
			break;

		    case 'n': // reject
			regfrm_reject(&forms[i], rejects[i], 0);
			break;

		    case 's': // skip
			// do nothing.
			break;

		    default:
			assert(0);
			break;
		}
	    }
	} // !dryrun

	// unlock all forms
	for (i = 0; i < cforms; i++)
	    regfrm_unlock(lfds[i]);

    } // while (ch != 'q')

    regq_end_pull(fpregq);
    LOG_IF(LOG_CONF_VALIDATE_REG,
           log_filef("log/validate_reg.log", LOG_CREAT,
                     "%s %s PAGE finished: %d forms\n",
                     Cdatelite(&now), cuser.userid, tid));
    // finishing
    clear(); move(5, 0);
    prints("您檢視了 %d 份註冊單。", tid);
    pressanykey();
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Regform UI
// 處理 Register Form
/////////////////////////////////////////////////////////////////////////////

int
m_register(void)
{
    FILE           *fn;
    int             x, y, wid, len;
    char            ans[4];
    char            genbuf[200];

    if (dashs(FN_REQLIST) <= 0) {
	outs("目前並無新註冊資料");
	return XEASY;
    }
    fn = fopen(FN_REQLIST, "r");
    assert(fn);

    vs_hdr("審核使用者註冊資料");
    y = 2;
    x = wid = 0;

    while (fgets(genbuf, STRLEN, fn) && x < 65) {
	move(y++, x);
	outs(genbuf);
	len = strlen(genbuf);
	if (len > wid)
	    wid = len;
	if (y >= t_lines - 3) {
	    y = 2;
	    x += wid + 2;
	}
    }

    fclose(fn);

    getdata(b_lines - 1, 0,
	    "開始審核嗎 (Y:單筆模式/N:不審/E:整頁模式/U:指定ID)？[N] ",
	    ans, sizeof(ans), LCECHO);

    if (ans[0] == 'y')
	regform2_validate_single(NULL);
    else if (ans[0] == 'e')
	regform2_validate_page(0);
    else if (ans[0] == 'u') {
	vs_hdr("指定審核");
	usercomplete(msg_uid, genbuf);
	if (genbuf[0])
	    regform2_validate_single(genbuf);
    }

    return 0;
}

/* vim:sw=4
 */
