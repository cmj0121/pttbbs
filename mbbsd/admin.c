#include "bbs.h"

/* 進站水球宣傳 */
int
m_loginmsg(void)
{
  char msg[100];
  move(21,0);
  clrtobot();
  if(SHM->loginmsg.pid && SHM->loginmsg.pid != currutmp->pid)
    {
      outs("目前已經有以下的 進站水球設定請先協調好再設定..");
      getmessage(SHM->loginmsg);
    }
  getdata(22, 0,
     "進站水球:本站活動,不干擾使用者為限,設定者離站自動取消,確定要設?(y/N)",
          msg, 3, LCECHO);

  if(msg[0]=='y' &&

     getdata_str(23, 0, "設定進站水球:", msg, 56, DOECHO, SHM->loginmsg.last_call_in))
    {
          SHM->loginmsg.pid=currutmp->pid; /*站長不多 就不管race condition */
          strlcpy(SHM->loginmsg.last_call_in, msg, sizeof(SHM->loginmsg.last_call_in));
          strlcpy(SHM->loginmsg.userid, cuser.userid, sizeof(SHM->loginmsg.userid));
    }
  return 0;
}

/* 使用者管理 */
int
m_user(void)
{
    userec_t        xuser;
    int             id;
    char            genbuf[200];

    vs_hdr("使用者設定");
    usercomplete(msg_uid, genbuf);
    if (*genbuf) {
	move(2, 0);
	if (user_info_admin(genbuf) < 0) {
	    outs(err_uid);
	    clrtoeol();
	    pressanykey();
	}
    }
    return 0;
}

int
user_info_admin(const char *userid)
{
    userec_t xuser;
    int id = getuser(userid, &xuser);
    if (!id)
	return -1;
    vs_hdr("�ϥΪ̳]�w");
    user_display(&xuser, 1);
    if (HasUserPerm(PERM_ACCOUNTS))
	uinfo_query(xuser.userid, 1, id);
    else
	pressanykey();
    return 0;
}

static int retrieve_backup(userec_t *user)
{
    int     uid;
    char    src[PATHLEN], dst[PATHLEN];
    char    ans;

    if ((uid = searchuser(user->userid, user->userid))) {
	userec_t orig;
	passwd_sync_query(uid, &orig);
	strlcpy(user->passwd, orig.passwd, sizeof(orig.passwd));
	setumoney(uid, user->money);
	passwd_sync_update(uid, user);
	return 0;
    }

    ans = vans("目前的 PASSWD 檔沒有此 ID，新增嗎？[y/N]");

    if (ans != 'y') {
	vmsg("目前的 PASSWDS 檔沒有此 ID，請先新增此帳號");
	return -1;
    }

    if (setupnewuser((const userec_t *)user) >= 0) {
	sethomepath(dst, user->userid);
	if (!dashd(dst)) {
	    snprintf(src, sizeof(src), "tmp/%s", user->userid);
	    if (!dashd(src) || !Rename(src, dst))
		mkuserdir(user->userid);
	}
	return 0;
    }
    return -1;
}

int
upgrade_passwd(userec_t *puser)
{
    if (puser->version == PASSWD_VERSION)
	return 1;
    if (!puser->userid[0])
	return 1;
    // unknown version
    return 0;

#if 0
    // this is a sample.
    if (puser->version == 2275) // chicken change
    {
	memset(puser->career,  0, sizeof(puser->career));
	memset(puser->chkpad0, 0, sizeof(puser->chkpad0));
	memset(puser->chkpad1, 0, sizeof(puser->chkpad1));
	memset(puser->chkpad2, 0, sizeof(puser->chkpad2));
	puser->lastseen= 0;
	puser->version = PASSWD_VERSION;
	return ;
    }
#endif
}

struct userec_filter_t;
typedef struct userec_filter_t userec_filter_t;
typedef const char *(*userec_filter_func)(userec_filter_t *, const userec_t *);

typedef struct {
    int field;
    char key[22];
} userec_keyword_t;

typedef struct {
    uint32_t userlevel_mask;
    uint32_t userlevel_wants;
    uint32_t role_mask;
    uint32_t role_wants;
} userec_perm_t;

enum {
    USEREC_REGTIME_Y,
    USEREC_REGTIME_YM,
    USEREC_REGTIME_YMD,
    USEREC_REGTIME_YMD_H,
    USEREC_REGTIME_YMD_HM,
    USEREC_REGTIME_YMD_HMS,
};

static const char * const userec_regtime_desc[] = {
    "查詢註冊時間 (年份)",
    "查詢註冊時間 (年月份)",
    "查詢註冊時間 (年月日)",
    "查詢註冊時間 (年月日 時)",
    "查詢註冊時間 (年月日 時分)",
    "查詢註冊時間 (年分日 時分秒)",
};

typedef struct {
    struct tm regtime;
    // See USEREC_REGTIME_* enum above.
    uint8_t match_type;
} userec_regtime_t;

#define USEREC_FILTER_KEYWORD (1)
#define USEREC_FILTER_PERM (2)
#define USEREC_FILTER_REGTIME (3)

struct userec_filter_t {
    int type;
    const char *(*filter)(struct userec_filter_t *, const userec_t *);
    const char *(*desc)(struct userec_filter_t *);
    union {
	userec_keyword_t keyword;
	userec_perm_t perm;
	userec_regtime_t regtime;
    };
};

static const char *
userec_filter_keyword_filter(userec_filter_t *uf, const userec_t *user)
{
    assert(uf->type == USEREC_FILTER_KEYWORD);
    const char * const key = uf->keyword.key;
    const int keytype = uf->keyword.field;
    if ((!keytype || keytype == 1) &&
	DBCS_strcasestr(user->userid, key))
	return user->userid;
    else if ((!keytype || keytype == 2) &&
	     DBCS_strcasestr(user->realname, key))
	return user->realname;
    else if ((!keytype || keytype == 3) &&
	     DBCS_strcasestr(user->nickname, key))
	return user->nickname;
    else if ((!keytype || keytype == 4) &&
	     DBCS_strcasestr(user->address, key))
	return user->address;
    else if ((!keytype || keytype == 5) &&
	     strcasestr(user->email, key)) // not DBCS.
	return user->email;
    else if ((!keytype || keytype == 6) &&
	     strcasestr(user->lasthost, key)) // not DBCS.
	return user->lasthost;
    else if ((!keytype || keytype == 7) &&
	     DBCS_strcasestr(user->career, key))
	return user->career;
    else if ((!keytype || keytype == 8) &&
	     DBCS_strcasestr(user->justify, key))
	return user->justify;
    return NULL;
}

static const char *
userec_filter_keyword_desc(userec_filter_t *uf)
{
    return uf->keyword.key;
}

static const char *
userec_filter_perm_filter(userec_filter_t *uf, const userec_t *user)
{
    assert(uf->type == USEREC_FILTER_PERM);
    if ((user->userlevel & uf->perm.userlevel_mask) ==
	uf->perm.userlevel_wants &&
	(user->role & uf->perm.role_mask) == uf->perm.role_wants)
	return "權限符合";
    return NULL;
}

static const char *
userec_filter_perm_desc(userec_filter_t *uf GCC_UNUSED)
{
    return "查詢帳號權限";
}

static const char *
userec_filter_regtime_filter(userec_filter_t *uf, const userec_t *user)
{
    assert(uf->type == USEREC_FILTER_REGTIME);
    const struct tm *regtime = &uf->regtime.regtime;
    struct tm _regtime;

    localtime4_r(&user->firstlogin, &_regtime);

    // Check date and time based on what the user specified
    switch (uf->regtime.match_type) {
	case USEREC_REGTIME_YMD_HMS:
	    if (regtime->tm_sec != _regtime.tm_sec)
		return NULL;
	    // fall through
	case USEREC_REGTIME_YMD_HM:
	    if (regtime->tm_min != _regtime.tm_min)
		return NULL;
	    // fall through
	case USEREC_REGTIME_YMD_H:
	    if (regtime->tm_hour != _regtime.tm_hour)
		return NULL;
	    // fall through
	case USEREC_REGTIME_YMD:
	    if (regtime->tm_mday != _regtime.tm_mday)
		return NULL;
	    // fall through
	case USEREC_REGTIME_YM:
	    if (regtime->tm_mon != _regtime.tm_mon)
		return NULL;
	    // fall through
	case USEREC_REGTIME_Y:
	    if (regtime->tm_year != _regtime.tm_year)
		return NULL;
	    return "註冊時間符合";
	default:
	    // Not reached
	    assert(0);
    }
}

static const char *
userec_filter_regtime_desc(userec_filter_t *uf)
{
    return userec_regtime_desc[uf->regtime.match_type];
}

static int
get_userec_filter(int row, userec_filter_t *uf)
{
    char tbuf[4];

    move(row, 0);
    outs("搜尋欄位: [0]全部 1.ID 2.姓名 3.暱稱 4.地址 5.Mail 6.IP 7.職業 8.認證\n");
    outs("          [a]不允許認證碼註冊 [b]註冊時間\n");
    row += 2;
    do {
	memset(uf, 0, sizeof(*uf));
	getdata(row++, 0, "要搜尋哪種資料？", tbuf, 2, DOECHO);

	if (strlen(tbuf) > 1)
	    continue;

	char sel = tbuf[0];
	if (!sel)
	    sel = '0';
	if (sel >= '0' && sel <= '8') {
	    uf->type = USEREC_FILTER_KEYWORD;
	    uf->filter = userec_filter_keyword_filter;
	    uf->desc = userec_filter_keyword_desc;
	    uf->keyword.field = sel - '0';
	    getdata(row++, 0, "請輸入關鍵字: ", uf->keyword.key,
		    sizeof(uf->keyword.key), DOECHO);
	    if (!uf->keyword.key[0])
		return -1;
	} else if (sel == 'a' || sel == 'A') {
	    uf->type = USEREC_FILTER_PERM;
	    uf->filter = userec_filter_perm_filter;
	    uf->desc = userec_filter_perm_desc;
	    uf->perm.userlevel_mask = uf->perm.userlevel_wants =
		PERM_NOREGCODE;
	} else if (sel == 'b' || sel == 'B') {
	    char buf[20], *p;

	    uf->type = USEREC_FILTER_REGTIME;
	    uf->filter = userec_filter_regtime_filter;
	    uf->desc = userec_filter_regtime_desc;

	    row += 3;
	    outs("註冊時間搜尋最少包含年分，最細可到秒，請依需求填入。\n");
	    outs("年月份請輸入 \"年/月\"；至分則輸入 \"年/月/日 時:分\"。\n");
	    outs("為避免誤判，搜尋細至小時時請填 \"年/月/日 時:\"。\n");
	    getdata(row++, 0, "請輸入註冊時間 (如 2019/12/31 23:59:59): ",
		    buf, sizeof(buf), DOECHO);

	    // check the returned pointer to make sure the match uses the whole string
	    if ((p = strptime(buf, "%Y/%m/%d %H:%M:%S", &uf->regtime.regtime)) && !*p)
		uf->regtime.match_type = USEREC_REGTIME_YMD_HMS;
	    else if ((p = strptime(buf, "%Y/%m/%d %H:%M", &uf->regtime.regtime)) && !*p)
		uf->regtime.match_type = USEREC_REGTIME_YMD_HM;
	    else if ((p = strptime(buf, "%Y/%m/%d %H:", &uf->regtime.regtime)) && !*p)
		uf->regtime.match_type = USEREC_REGTIME_YMD_H;
	    else if ((p = strptime(buf, "%Y/%m/%d", &uf->regtime.regtime)) && !*p)
		uf->regtime.match_type = USEREC_REGTIME_YMD;
	    else if ((p = strptime(buf, "%Y/%m", &uf->regtime.regtime)) && !*p)
		uf->regtime.match_type = USEREC_REGTIME_YM;
	    else if ((p = strptime(buf, "%Y", &uf->regtime.regtime)) && !*p)
		uf->regtime.match_type = USEREC_REGTIME_Y;
	    else {
		outs("時間格式錯誤\n");
		return -1;
	    }
	}
    } while (!uf->type);
    return 0;
}

#define MAX_USEREC_FILTERS (5)

static int
search_key_user(const char *passwdfile, int mode)
{
    userec_t        user;
    int             ch;
    int             unum = 0;
    FILE            *fp1 = fopen(passwdfile, "r");
    char            friendfile[PATHLEN]="";
    const char	    *keymatch;
    int	            isCurrentPwd;
    userec_filter_t ufs[MAX_USEREC_FILTERS];
    size_t	    num_ufs = 0;

    assert(fp1);

    isCurrentPwd = (strcmp(passwdfile, FN_PASSWD) == 0);
    clear();
    if (!mode) {
	userec_filter_t *uf = &ufs[0];
	uf->type = 1;
	uf->filter = userec_filter_keyword_filter;
	uf->desc = userec_filter_keyword_desc;
	uf->keyword.field = 1;
	getdata(0, 0, "請輸入id :", uf->keyword.key, sizeof(uf->keyword.key),
		DOECHO);
	num_ufs++;
    } else {
	// improved search
	vs_hdr("關鍵字搜尋");
	if (!get_userec_filter(1, &ufs[0]))
	    num_ufs++;
    }

    if (!num_ufs) {
	fclose(fp1);
	return 0;
    }
    const char *desc = ufs[0].desc(&ufs[0]);
    vs_hdr(desc);

    // <= or < ? I'm not sure...
    while ((fread(&user, sizeof(user), 1, fp1)) > 0 && unum++ < MAX_USERS) {

	// skip empty records
	if (!user.userid[0])
	    continue;

	if (!(unum & 0xFF)) {
	    vs_hdr(desc);
	    prints("第 [%d] 筆資料\n", unum);
	    refresh();
	}

	// XXX 這裡會取舊資料，要小心 PWD 的 upgrade
	if (!upgrade_passwd(&user))
	    continue;

	for (size_t i = 0; i < num_ufs; i++) {
	    keymatch = ufs[i].filter(&ufs[i], &user);
	    if (!keymatch)
		break;
	}
        while (keymatch) {
	    vs_hdr(desc);
	    prints("第 [%d] 筆資料\n", unum);
	    refresh();

	    user_display(&user, 1);
	    // user_display does not have linefeed in tail.

	    if (isCurrentPwd && HasUserPerm(PERM_ACCOUNTS))
		uinfo_query(user.userid, 1, unum);
	    else
		outs("\n");

	    // XXX don't trust 'user' variable after here
	    // because uinfo_query may have changed it.

	    vs_footer("  搜尋帳號  ", mode ?
		      "  (空白鍵)搜尋下一個 (A)加入名單 (F)新增條件 (Q)離開" :
		      "  (空白鍵)搜尋下一個 (A)加入名單 (S)取用備份資料 "
		      "(Q)離開");

	    while ((ch = vkey()) == 0)
		;
	    if (ch == 'a' || ch == 'A') {
		if(!friendfile[0])
		{
		    friend_special();
		    setfriendfile(friendfile, FRIEND_SPECIAL);
		}
		friend_add(user.userid, FRIEND_SPECIAL, keymatch);
		break;
	    }
	    if (mode && (ch == 'f' || ch == 'F')) {
		if (num_ufs >= MAX_USEREC_FILTERS) {
		    vmsg("搜尋條件數量已達上限。");
		    continue;
		}
		clear();
		vs_hdr("新增條件");
		if (get_userec_filter(1, &ufs[num_ufs])) {
		    vmsg("新增條件失敗");
		    continue;
		}
		num_ufs++;
		desc = "多重條件";
		break;
	    }
	    if (ch == ' ')
		break;
	    if (ch == 'q' || ch == 'Q') {
		fclose(fp1);
		return 0;
	    }
	    if (ch == 's' && !mode) {
		if (retrieve_backup(&user) >= 0) {
		    fclose(fp1);
		    vmsg("已成功取用備份資料。");
		    return 0;
		} else {
		    vmsg("錯誤: 取用備份資料失敗。");
		}
	    }
	}
    }

    fclose(fp1);
    return 0;
}

/* 以任意 key 尋找使用者 */
int
search_user_bypwd(void)
{
    search_key_user(FN_PASSWD, 1);
    return 0;
}

/* 尋找備份的使用者資料 */
int
search_user_bybakpwd(void)
{
    char           *choice[] = {
	"PASSWDS.NEW1", "PASSWDS.NEW2", "PASSWDS.NEW3",
	"PASSWDS.NEW4", "PASSWDS.NEW5", "PASSWDS.NEW6",
	"PASSWDS.BAK"
    };
    int             ch;

    clear();
    move(1, 1);
    outs("請輸入你要用來尋找備份的檔案 或按 'q' 離開\n");
    outs(" [" ANSI_COLOR(1;31) "1" ANSI_RESET "]一天前,"
	 " [" ANSI_COLOR(1;31) "2" ANSI_RESET "]兩天前,"
	 " [" ANSI_COLOR(1;31) "3" ANSI_RESET "]三天前\n");
    outs(" [" ANSI_COLOR(1;31) "4" ANSI_RESET "]四天前,"
	 " [" ANSI_COLOR(1;31) "5" ANSI_RESET "]五天前,"
	 " [" ANSI_COLOR(1;31) "6" ANSI_RESET "]六天前\n");
    outs(" [7]備份的\n");
    do {
	move(5, 1);
	outs("選擇 => ");
	ch = vkey();
	if (ch == 'q' || ch == 'Q')
	    return 0;
    } while (ch < '1' || ch > '7');
    ch -= '1';
    if( access(choice[ch], R_OK) != 0 )
	vmsg("檔案不存在");
    else
	search_key_user(choice[ch], 0);
    return 0;
}

static void
bperm_msg(const boardheader_t * board)
{
    prints("\n設定 [%s] 看板之(%s)權限：", board->brdname,
	   board->brdattr & BRD_POSTMASK ? "發表" : "閱讀");
}

unsigned int
setperms(unsigned int pbits, const char * const pstring[])
{
    register int    i;

    move(4, 0);
    for (i = 0; i < NUMPERMS / 2; i++) {
	prints("%c. %-20s %-15s %c. %-20s %s\n",
	       'A' + i, pstring[i],
	       ((pbits >> i) & 1 ? "ˇ" : "Ｘ"),
	       i < 10 ? 'Q' + i : '0' + i - 10,
	       pstring[i + 16],
	       ((pbits >> (i + 16)) & 1 ? "ˇ" : "Ｘ"));
    }
    clrtobot();
    while (
       (i = vmsg("請按 [A-5] 切換設定，按 [Return] 結束："))!='\r')
    {
	if (isdigit(i))
	    i = i - '0' + 26;
	else if (isalpha(i))
	    i = tolower(i) - 'a';
	else {
	    bell();
	    continue;
	}

	pbits ^= (1 << i);
	move(i % 16 + 4, i <= 15 ? 24 : 64);
	outs((pbits >> i) & 1 ? "ˇ" : "Ｘ");
    }
    return pbits;
}

#ifdef CHESSCOUNTRY
static void
AddingChessCountryFiles(const char* apath)
{
    char filename[PATHLEN];
    char symbolicname[PATHLEN];
    char adir[PATHLEN];
    FILE* fp;
    fileheader_t fh;

    setadir(adir, apath);

    /* creating chess country regalia */
    snprintf(filename, sizeof(filename), "%s/chess_ensign", apath);
    close(OpenCreate(filename, O_WRONLY));

    strlcpy(symbolicname, apath, sizeof(symbolicname));
    stampfile(symbolicname, &fh);
    symlink("chess_ensign", symbolicname);

    strcpy(fh.title, "◇ 棋國國徽 (不能刪除，系統需要)");
    strcpy(fh.owner, str_sysop);
    append_record(adir, &fh, sizeof(fileheader_t));

    /* creating member list */
    snprintf(filename, sizeof(filename), "%s/chess_list", apath);
    if (!dashf(filename)) {
	fp = fopen(filename, "w");
	assert(fp);
	fputs("棋國國名\n"
		"帳號            階級    加入日期        等級或被誰俘虜\n"
		"──────    ───  ─────      ───────\n",
		fp);
	fclose(fp);
    }

    strlcpy(symbolicname, apath, sizeof(symbolicname));
    stampfile(symbolicname, &fh);
    symlink("chess_list", symbolicname);

    strcpy(fh.title, "◇ 棋國成員表 (不能刪除，系統需要)");
    strcpy(fh.owner, str_sysop);
    append_record(adir, &fh, sizeof(fileheader_t));

    /* creating profession photos' dir */
    snprintf(filename, sizeof(filename), "%s/chess_photo", apath);
    Mkdir(filename);

    strlcpy(symbolicname, apath, sizeof(symbolicname));
    stampfile(symbolicname, &fh);
    symlink("chess_photo", symbolicname);

    strcpy(fh.title, "◆ 棋國照片檔 (不能刪除，系統需要)");
    strcpy(fh.owner, str_sysop);
    append_record(adir, &fh, sizeof(fileheader_t));
}
#endif /* defined(CHESSCOUNTRY) */

/* 自動設立精華區 */
void
setup_man(const boardheader_t * board, const boardheader_t * oldboard)
{
    char            genbuf[200];

    setapath(genbuf, board->brdname);
    Mkdir(genbuf);

#ifdef CHESSCOUNTRY
    if (oldboard == NULL || oldboard->chesscountry != board->chesscountry)
	if (board->chesscountry != CHESSCODE_NONE)
	    AddingChessCountryFiles(genbuf);
	// else // doesn't remove files..
#endif
}

void delete_board_link(boardheader_t *bh, int bid)
{
    assert(0<=bid-1 && bid-1<MAX_BOARD);
    memset(bh, 0, sizeof(boardheader_t));
    substitute_record(fn_board, bh, sizeof(boardheader_t), bid);
    reset_board(bid);
    sort_bcache();
    log_usies("DelLink", bh->brdname);
}

int dir_cmp(const void *a, const void *b)
{
  return (atoi( &((fileheader_t *)a)->filename[2] ) -
          atoi( &((fileheader_t *)b)->filename[2] ));
}

void merge_dir(const char *dir1, const char *dir2, int isoutter)
{
     int i, pn, sn;
     fileheader_t *fh;
     char *p1, *p2, bakdir[128], file1[128], file2[128];
     strcpy(file1,dir1);
     strcpy(file2,dir2);
     if((p1=strrchr(file1,'/')))
	 p1 ++;
     else
	 p1 = file1;
     if((p2=strrchr(file2,'/')))
	 p2 ++;
     else
	 p2 = file2;

     pn=get_num_records(dir1, sizeof(fileheader_t));
     sn=get_num_records(dir2, sizeof(fileheader_t));
     if(!sn) return;
     fh= (fileheader_t *)malloc( (pn+sn)*sizeof(fileheader_t));
     get_records(dir1, fh, sizeof(fileheader_t), 1, pn);
     get_records(dir2, fh+pn, sizeof(fileheader_t), 1, sn);
     if(isoutter)
         {
             for(i=0; i<sn; i++)
               if(fh[pn+i].owner[0])
                   strcat(fh[pn+i].owner, ".");
         }
     qsort(fh, pn+sn, sizeof(fileheader_t), dir_cmp);
     snprintf(bakdir, sizeof(bakdir), "%s.bak", dir1);
     Rename(dir1, bakdir);
     for(i=1; i<=pn+sn; i++ )
        {
         if(!fh[i-1].filename[0]) continue;
         if(i == pn+sn ||  strcmp(fh[i-1].filename, fh[i].filename))
	 {
                fh[i-1].recommend =0;
		fh[i-1].filemode |= 1;
                append_record(dir1, &fh[i-1], sizeof(fileheader_t));
		strcpy(p1, fh[i-1].filename);
                if(!dashf(file1))
		      {
			  strcpy(p2, fh[i-1].filename);
			  Copy(file2, file1);
		      }
	 }
         else
                fh[i].filemode |= fh[i-1].filemode;
        }

     free(fh);
}

int
m_mod_board(char *bname)
{
    boardheader_t   bh, newbh;
    int             bid;
    char            genbuf[PATHLEN], ans[4];
    int y;

    bid = getbnum(bname);
    if (!bid || !bname[0] || get_record(fn_board, &bh, sizeof(bh), bid) == -1) {
	vmsg(err_bid);
	return -1;
    }
    assert(0<=bid-1 && bid-1<MAX_BOARD);
    prints("看板名稱：%s %s\n看板說明：%s\n看板bid：%d\n看板GID：%d\n"
	   "板主名單：%s", bh.brdname, (bh.brdattr & BRD_NOCREDIT) ?
           ANSI_COLOR(1;31) "[已設定發文無文章金錢獎勵]" ANSI_RESET : "",
           bh.title, bid, bh.gid, bh.BM);
    bperm_msg(&bh);

    /* Ptt 這邊斷行會檔到下面 */
    move(9, 0);
    if (bh.brdattr & BRD_SYMBOLIC) {
        snprintf(genbuf, sizeof(genbuf), "[看板連結] (D)刪除 [Q]取消? ");
    } else {
        snprintf(genbuf, sizeof(genbuf), "(E)�]�w (V)�o����y%s%s [Q]����? ",
                 HasUserPerm(PERM_BOARD) ? " (B)Vote (S)�Ϧ^ (C)�X�� (G)�ֳz�ѥd" : "",
                 HasUserPerm(PERM_SYSSUBOP | PERM_SYSSUPERSUBOP | PERM_BOARD) ? " (D)�R��" : "");
    }
    getdata(10, 0, genbuf, ans, 3, LCECHO);
    if (isascii(*ans))
        *ans = tolower(*ans);

    if ((bh.brdattr & BRD_SYMBOLIC) && *ans) {
        switch (*ans) {
            case 'd':
            case 'q':
                break;
            default:
                vmsg("禁止更動連結看板，請直接修正原看板");
                break;
        }
    }

    switch (*ans) {
    case 'g':
	if (HasUserPerm(PERM_BOARD)) {
	    char            path[PATHLEN];
	    setbfile(genbuf, bname, FN_TICKET_LOCK);
	    setbfile(path, bname, FN_TICKET_END);
	    rename(genbuf, path);
	}
	break;
    case 's':
	if (HasUserPerm(PERM_BOARD)) {
	  snprintf(genbuf, sizeof(genbuf),
		   BBSHOME "/bin/buildir boards/%c/%s &",
		   bh.brdname[0], bh.brdname);
	    system(genbuf);
	}
	break;
    case 'c':
	if (HasUserPerm(PERM_BOARD)) {
	   char frombname[20], fromdir[PATHLEN];
	    CompleteBoard(MSG_SELECT_BOARD, frombname);
            if (frombname[0] == '\0' || !getbnum(frombname) ||
		!strcmp(frombname,bname))
	                     break;
            setbdir(genbuf, bname);
            setbdir(fromdir, frombname);
            merge_dir(genbuf, fromdir, 0);
	    touchbtotal(bid);
	}
	break;
    case 'b':
	if (HasUserPerm(PERM_BOARD)) {
	    char            bvotebuf[10];

	    memcpy(&newbh, &bh, sizeof(bh));
	    snprintf(bvotebuf, sizeof(bvotebuf), "%d", newbh.bvote);
	    move(20, 0);
	    prints("看板 %s 原來的 BVote：%d", bh.brdname, bh.bvote);
	    getdata_str(21, 0, "新的 Bvote：", genbuf, 5, NUMECHO, bvotebuf);
	    newbh.bvote = atoi(genbuf);
	    assert(0<=bid-1 && bid-1<MAX_BOARD);
	    substitute_record(fn_board, &newbh, sizeof(newbh), bid);
	    reset_board(bid);
	    log_usies("SetBoardBvote", newbh.brdname);
	    break;
	} else
	    break;
    case 'v':
	memcpy(&newbh, &bh, sizeof(bh));
	outs("看板發文的文章金錢獎勵方法目前為");
	outs((bh.brdattr & BRD_NOCREDIT) ?
             ANSI_COLOR(1;31) "禁止" ANSI_RESET : "正常");
	getdata(21, 0, "確定更改？", genbuf, 5, LCECHO);
	if (genbuf[0] == 'y') {
            newbh.brdattr ^= BRD_NOCREDIT;
	    assert(0<=bid-1 && bid-1<MAX_BOARD);
	    substitute_record(fn_board, &newbh, sizeof(newbh), bid);
	    reset_board(bid);
	    log_usies("ViolateLawSet", newbh.brdname);
	}
	break;
    case 'd':
	if (!(HasUserPerm(PERM_BOARD) ||
		    (HasUserPerm(PERM_SYSSUPERSUBOP) && GROUPOP())))
	    break;
	getdata_str(9, 0, msg_sure_ny, genbuf, 3, LCECHO, "N");
	if (genbuf[0] != 'y' || !bname[0])
	    outs(MSG_DEL_CANCEL);
	else if (bh.brdattr & BRD_SYMBOLIC) {
	    delete_board_link(&bh, bid);
	}
	else {
	    strlcpy(bname, bh.brdname, sizeof(bh.brdname));
	    snprintf(genbuf, sizeof(genbuf),
		    "/bin/tar zcvf tmp/board_%s.tgz boards/%c/%s man/boards/%c/%s >/dev/null 2>&1;"
		    "/bin/rm -fr boards/%c/%s man/boards/%c/%s",
		    bname, bname[0], bname, bname[0],
		    bname, bname[0], bname, bname[0], bname);
	    system(genbuf);
	    memset(&bh, 0, sizeof(bh));
	    snprintf(bh.title, sizeof(bh.title),
		     "     %s 看板 %s 刪除", bname, cuser.userid);
	    post_msg(BN_SECURITY, bh.title, "請注意刪除的合法性", "[系統安全局]");
	    assert(0<=bid-1 && bid-1<MAX_BOARD);
	    substitute_record(fn_board, &bh, sizeof(bh), bid);
	    reset_board(bid);
            sort_bcache();
	    log_usies("DelBoard", bh.title);
	    outs("刪板完畢");
	}
	break;
    case 'e':
        y = 8;
	move(y++, 0); clrtobot();
	outs("直接按 [Return] 不修改該項設定");
	memcpy(&newbh, &bh, sizeof(bh));

	while (getdata(y, 0, "新看板名稱：", genbuf, IDLEN + 1, DOECHO)) {
	    if (getbnum(genbuf)) {
                mvouts(y + 1, 0, "錯誤: 此新看板名已存在\n");
            } else if ( !is_valid_brdname(genbuf) ) {
                mvouts(y + 1, 0, "錯誤: 無法使用此名稱。"
                       "請使用英數字或 _-. 且開頭不得為數字。\n");
            } else {
                if (genbuf[0] != bh.brdname[0]) {
                    // change to 0 if you want to force permission when renaming
                    // with different initial character.
                    const int free_rename = 1;
                    if (free_rename || HasUserPerm(PERM_BOARD)) {
                        mvouts(y + 1, 0, ANSI_COLOR(1;31)
                                "警告: 看板首字母不同,大看板改名會非常久,"
                                "千萬不可中途斷線否則看板會壞掉"
                                ANSI_RESET "\n");
                    } else {
                        mvouts(y + 1, 0,
                                "錯誤: 新舊名稱第一個字母若不同(大小寫有別)"
                                "要看板總管以上等級才可設定\n");
                        continue;
                    }
                }
		strlcpy(newbh.brdname, genbuf, sizeof(newbh.brdname));
		break;
	    }
	}
        y++;

	do {
	    getdata_str(y, 0, "看板類別：", genbuf, 5, DOECHO, bh.title);
	    if (strlen(genbuf) == 4)
		break;
	} while (1);
        y++;

	strcpy(newbh.title, genbuf);
	newbh.title[4] = ' ';

	// 7 for category
	getdata_str(y++, 0, "看板主題：", genbuf, BTLEN + 1 -7,
                    DOECHO, bh.title + 7);
	if (genbuf[0])
	    strlcpy(newbh.title + 7, genbuf, sizeof(newbh.title) - 7);

        do {
            int uids[MAX_BMs], i;
            if (!getdata_str(y, 0, "新板主名單：", genbuf, IDLEN * 3 + 3,
                             DOECHO, bh.BM) || strcmp(genbuf, bh.BM) == 0)
                break;
            // TODO 照理來說在這裡 normalize 一次比較好；可惜目前似乎有奇怪的
            // 代管制度，會有人把 BM list 設定 [ ...... / some_uid]，就會變成
            // 一面徵求板主同時又有人(maybe小組長)有管理權限而且還不顯示出來。
            // 這設計很糟糕，也無法判斷是不小心誤設(多了空白)或是故意的，再者
            // 還有人以為這裡打句英文很帥氣結果造成該英文的ID意外獲得權限。
            // 未來應該整個取消，完全 normalize。
	    trim(genbuf);
            parseBMlist(genbuf, uids);
            mvouts(y + 2, 0, " 實際生效的板主ID: " ANSI_COLOR(1));
            for (i = 0; i < MAX_BMs && uids[i] >= 0; i++) {
                prints("%s ", getuserid(uids[i]));
            }
            outs(ANSI_RESET "\n");

            // ref: does_board_have_public_bm
            if (genbuf[0] <= ' ')
                outs(ANSI_COLOR(1;31)
                     " *** 因為開頭是空白或中文, 看板內左上角或按i的"
                     "板主名單會顯示為徵求中或無 ***"
                     ANSI_RESET "\n");
            mvprints(y+5, 0, "注意: 資源回收筒與編輯歷史已不用先把自己設成板主。\n"
                     "詳情請見(X)->(L)系統更新記錄。\n");
            if (getdata(y + 4, 0, "確定此板主名單正確?[y/N] ", ans,
                        sizeof(ans), LCECHO) &&
                ans[0] == 'y') {
                strlcpy(newbh.BM, genbuf, sizeof(newbh.BM));
                move(y + 1, 0); clrtobot();
                break;
            }
            move(y, 0); clrtobot();
        } while (1);
        y++;

#ifdef CHESSCOUNTRY
	if (HasUserPerm(PERM_BOARD)) {
	    snprintf(genbuf, sizeof(genbuf), "%d", bh.chesscountry);
	    if (getdata_str(y++, 0,
			"設定棋國 (0)無 (1)五子棋 (2)象棋 (3)圍棋 (4) 黑白棋",
			ans, sizeof(ans), NUMECHO, genbuf)){
		newbh.chesscountry = atoi(ans);
		if (newbh.chesscountry > CHESSCODE_MAX ||
			newbh.chesscountry < CHESSCODE_NONE)
		    newbh.chesscountry = bh.chesscountry;
	    }
	}
#endif /* defined(CHESSCOUNTRY) */

	if (HasUserPerm(PERM_BOARD)) {
	    move(1, 0);
	    clrtobot();
	    newbh.brdattr = setperms(newbh.brdattr, str_permboard);
	    move(1, 0);
	    clrtobot();
	}

	{
	    const char* brd_symbol;
	    if (newbh.brdattr & BRD_GROUPBOARD)
        	brd_symbol = "Σ";
	    else
		brd_symbol = "◎";

	    newbh.title[5] = brd_symbol[0];
	    newbh.title[6] = brd_symbol[1];
	}

	if (HasUserPerm(PERM_BOARD) && !(newbh.brdattr & BRD_HIDE)) {
            getdata(y++, 0, "�]�wŪ�g�v��(y/N)�H", ans, sizeof(ans), LCECHO);
	    if (*ans == 'y') {
		getdata_str(y++, 0, "限制 [R]閱讀 (P)發表？", ans, sizeof(ans), LCECHO,
			    "R");
		if (*ans == 'p')
		    newbh.brdattr |= BRD_POSTMASK;
		else
		    newbh.brdattr &= ~BRD_POSTMASK;

		move(1, 0);
		clrtobot();
		bperm_msg(&newbh);
		newbh.level = setperms(newbh.level, str_permid);
		clear();
	    }
	}

	getdata(b_lines - 1, 0, "請您確定(Y/N)？[Y]", genbuf, 4, LCECHO);

	if ((*genbuf != 'n') && memcmp(&newbh, &bh, sizeof(bh))) {
	    char buf[64];

	    if (strcmp(bh.brdname, newbh.brdname)) {
		char            src[60], tar[60];

		setbpath(src, bh.brdname);
		setbpath(tar, newbh.brdname);
		Rename(src, tar);

		setapath(src, bh.brdname);
		setapath(tar, newbh.brdname);
		Rename(src, tar);
	    }
	    setup_man(&newbh, &bh);
	    assert(0<=bid-1 && bid-1<MAX_BOARD);
	    substitute_record(fn_board, &newbh, sizeof(newbh), bid);
	    reset_board(bid);
            sort_bcache();
	    log_usies("SetBoard", newbh.brdname);

	    snprintf(buf, sizeof(buf), "[看板變更] %s (by %s)", bh.brdname, cuser.userid);
	    snprintf(genbuf, sizeof(genbuf),
		    "板名: %s => %s\n"
		    "板主: %s => %s\n",
		    bh.brdname, newbh.brdname, bh.BM, newbh.BM);
	    post_msg(BN_SECURITY, buf, genbuf, "[系統安全局]");
	}
    }
    return 0;
}

/* 設定看板 */
int
m_board(void)
{
    char            bname[32];

    vs_hdr("看板設定");
    CompleteBoardAndGroup(msg_bid, bname);
    if (!*bname)
	return 0;
    m_mod_board(bname);
    return 0;
}

/* 設定系統檔案 */
int
x_file(void)
{
    return psb_admin_edit();
}

static int add_board_record(const boardheader_t *board)
{
    int bid;
    if ((bid = getbnum("")) > 0) {
	assert(0<=bid-1 && bid-1<MAX_BOARD);
	substitute_record(fn_board, board, sizeof(boardheader_t), bid);
	reset_board(bid);
        sort_bcache();
    } else if (SHM->Bnumber >= MAX_BOARD) {
	return -1;
    } else if (append_record(fn_board, (fileheader_t *)board, sizeof(boardheader_t)) == -1) {
	return -1;
    } else {
	addbrd_touchcache();
    }
    return 0;
}

/**
 * open a new board
 * @param whatclass In which sub class
 * @param recover   Forcely open a new board, often used for recovery.
 * @return -1 if failed
 */
int
m_newbrd(int whatclass, int recover)
{
    boardheader_t   newboard;
    char            ans[4];
    char            genbuf[200];

    vs_hdr("建立新板");
    memset(&newboard, 0, sizeof(newboard));

    newboard.gid = whatclass;
    if (newboard.gid == 0) {
	vmsg("請先選擇一個類別再開板!");
	return -1;
    }
    do {
	if (!getdata(3, 0, msg_bid, newboard.brdname,
		     sizeof(newboard.brdname), DOECHO))
	    return -1;
        if (is_valid_brdname(newboard.brdname))
            break;
        // some i* need to be acknowledged
        vmsg("無法使用此名稱，請使用英數字或 _-. 且開頭不得為數字。");
    } while (true);

    do {
	getdata(6, 0, "看板類別：", genbuf, 5, DOECHO);
	if (strlen(genbuf) == 4)
	    break;
    } while (1);

    strcpy(newboard.title, genbuf);
    newboard.title[4] = ' ';

    getdata(8, 0, "看板主題：", genbuf, BTLEN + 1, DOECHO);
    if (genbuf[0])
	strlcpy(newboard.title + 7, genbuf, sizeof(newboard.title) - 7);
    setbpath(genbuf, newboard.brdname);

    // Recover 應只拿來處理目錄已存在(但.BRD沒有)的情況，不然就會在
    // 有人誤用時造成同個目錄有多個 board entry 的情形。
    // getbnum(newboard.brdname) > 0 時由於目前設計是會 new board,
    // 所以真的開板後只會造成 bcache 錯亂，不可不慎。
    if (getbnum(newboard.brdname) > 0) {
	vmsg("此看板已經存在! 請取不同英文板名");
	return -1;
    }
    if (Mkdir(genbuf) != 0) {
        if (errno == EEXIST) {
            if (!recover) {
                vmsg("看板目錄已存在，若是要修復看板請用 R 指令。");
                return -1;
            }
        } else {
            vmsgf("系統錯誤 #%d, 無法建立看板目錄。", errno);
            return -1;
        }
    }
    newboard.brdattr = 0;
#ifdef DEFAULT_AUTOCPLOG
    newboard.brdattr |= BRD_CPLOG;
#endif

    if (HasUserPerm(PERM_BOARD)) {
	move(1, 0);
	clrtobot();
	newboard.brdattr = setperms(newboard.brdattr, str_permboard);
	move(1, 0);
	clrtobot();
    }
    getdata(9, 0, "是看板? (N:目錄) (Y/n)：", genbuf, 3, LCECHO);
    if (genbuf[0] == 'n')
    {
	newboard.brdattr |= BRD_GROUPBOARD;
	newboard.brdattr &= ~BRD_CPLOG;
    }

	{
	    const char* brd_symbol;
	    if (newboard.brdattr & BRD_GROUPBOARD)
        	brd_symbol = "Σ";
	    else
		brd_symbol = "◎";

	    newboard.title[5] = brd_symbol[0];
	    newboard.title[6] = brd_symbol[1];
	}

    newboard.level = 0;
    getdata(11, 0, "板主名單：", newboard.BM, sizeof(newboard.BM), DOECHO);
#ifdef CHESSCOUNTRY
    if (getdata_str(12, 0, "設定棋國 (0)無 (1)五子棋 (2)象棋 (3)圍棋", ans,
		sizeof(ans), LCECHO, "0")){
	newboard.chesscountry = atoi(ans);
	if (newboard.chesscountry > CHESSCODE_MAX ||
		newboard.chesscountry < CHESSCODE_NONE)
	    newboard.chesscountry = CHESSCODE_NONE;
    }
#endif /* defined(CHESSCOUNTRY) */

    if (HasUserPerm(PERM_BOARD) && !(newboard.brdattr & BRD_HIDE)) {
	getdata_str(14, 0, "�]�wŪ�g�v��(Y/N)�H", ans, sizeof(ans), LCECHO, "N");
	if (*ans == 'y') {
	    getdata_str(15, 0, "限制 [R]閱讀 (P)發表？", ans, sizeof(ans), LCECHO, "R");
	    if (*ans == 'p')
		newboard.brdattr |= BRD_POSTMASK;
	    else
		newboard.brdattr &= (~BRD_POSTMASK);

	    move(1, 0);
	    clrtobot();
	    bperm_msg(&newboard);
	    newboard.level = setperms(newboard.level, str_permid);
	    clear();
	}
    }

    if (add_board_record(&newboard) < 0) {
	if (SHM->Bnumber >= MAX_BOARD)
	    vmsg("看板已滿，請洽系統站長");
	else
    	    vmsg("看板寫入失敗");

	setbpath(genbuf, newboard.brdname);
	rmdir(genbuf);
	return -1;
    }

    getbcache(whatclass)->childcount = 0;
    pressanykey();
    setup_man(&newboard, NULL);
    outs("\n新板成立");
    post_newboard(newboard.title, newboard.brdname, newboard.BM);
    log_usies("NewBoard", newboard.title);
    pressanykey();
    return 0;
}

int make_board_link(const char *bname, int gid)
{
    boardheader_t   newboard;
    int bid;

    bid = getbnum(bname);
    if(bid==0) return -1;
    assert(0<=bid-1 && bid-1<MAX_BOARD);
    memset(&newboard, 0, sizeof(newboard));

    /*
     * known issue:
     *   These two stuff will be used for sorting.  But duplicated brdnames
     *   may cause wrong binary-search result.  So I replace the last
     *   letters of brdname to '~'(ascii code 126) in order to correct the
     *   resuilt, thought I think it's a dirty solution.
     *
     *   Duplicate entry with same brdname may cause wrong result, if
     *   searching by key brdname.  But people don't need to know a board
     *   is a link, so just let SYSOP know it. You may want to read
     *   board.c:load_boards().
     */

    strlcpy(newboard.brdname, bname, sizeof(newboard.brdname));
    newboard.brdname[strlen(bname) - 1] = '~';
    strlcpy(newboard.title, bcache[bid - 1].title, sizeof(newboard.title));
    strcpy(newboard.title + 5, "＠看板連結");

    newboard.gid = gid;
    BRD_LINK_TARGET(&newboard) = bid;
    newboard.brdattr = BRD_SYMBOLIC;

    if (add_board_record(&newboard) < 0)
	return -1;
    return bid;
}

int make_board_link_interactively(int gid)
{
    char buf[32];
    vs_hdr("建立看板連結");

    outs("\n\n請注意: 看板連結會導致連結所在的群組之小組長一樣有群組管理權限。\n"
         "(例，在群組 A [小組長: abc]下建立了通往群組 B 的看板 C 的連結，\n"
         " 結果會導致 abc 在進入看板 C 時也有群組管理權限。)\n\n"
         "這是已知現象而且無解。在建立看板時請確定您已了解可能會發生的問題。\n");

    if (tolower(vmsg("確定要建立新看板連結嗎？ [y/N]: ")) != 'y')
        return -1;

    CompleteBoard(msg_bid, buf);
    if (!buf[0])
	return -1;


    if (make_board_link(buf, gid) < 0) {
	vmsg("看板連結建立失敗");
	return -1;
    }
    log_usies("NewSymbolic", buf);
    return 0;
}

static void
adm_give_id_money(const char *user_id, int money, const char *mail_title)
{
    char tt[TTLEN + 1] = {0};
    int  unum = searchuser(user_id, NULL);

    // XXX 站長們似乎利用這個功能來同時發錢或扣錢，return value 可能是 0
    // (若代表對方錢被扣光)
    if (unum <= 0 || pay_as_uid(unum, -money, "站長%s: %s",
                                money >= 0 ? "發紅包" : "扣錢",
                                mail_title) < 0) {
	move(12, 0);
	clrtoeol();
	prints("id:%s money:%d 不對吧!!", user_id, money);
	pressanykey();
    } else {
	snprintf(tt, sizeof(tt), "%s : %d " MONEYNAME, mail_title, money);
	mail_id(user_id, tt, "etc/givemoney.why", "[" BBSMNAME "銀行]");
    }
}

int
give_money(void)
{
    FILE           *fp, *fp2;
    char           *ptr, *id, *mn;
    char            buf[200] = "", reason[100], tt[TTLEN + 1] = "";
    int             to_all = 0, money = 0;
    int             total_money=0, count=0;

    getdata(0, 0, "指定使用者(S) 全站使用者(A) 取消(Q)？[S]", buf, 3, LCECHO);
    if (buf[0] == 'q')
	return 1;
    else if (buf[0] == 'a') {
	to_all = 1;
	getdata(1, 0, "發多少錢呢?", buf, 20, DOECHO);
	money = atoi(buf);
	if (money <= 0) {
	    move(2, 0);
	    vmsg("輸入錯誤!!");
	    return 1;
	}
    } else {
	if (veditfile("etc/givemoney.txt") < 0)
	    return 1;
    }

    clear();

    // check money:
    // 1. exceed limit
    // 2. id unique

    unlink("etc/givemoney.log");
    if (!(fp2 = fopen("etc/givemoney.log", "w")))
	return 1;

    getdata(0, 0, "動用國庫!請輸入正當理由(如活動名稱):", reason, 40, DOECHO);
    fprintf(fp2,"\n使用理由: %s\n", reason);

    getdata(1, 0, "要發錢了嗎(Y/N)[N]", buf, 3, LCECHO);
    if (buf[0] != 'y') {
        fclose(fp2);
	return 1;
    }

    getdata(1, 0, "紅包袋標題 ：", tt, TTLEN, DOECHO);
    fprintf(fp2,"\n紅包袋標題: %s\n", tt);
    move(2, 0);

    vmsg("編紅包袋內容");
    if (veditfile("etc/givemoney.why") < 0) {
        fclose(fp2);
	return 1;
    }

    vs_hdr("發錢中...");
    if (to_all) {
	int             i, unum;
	for (unum = SHM->number, i = 0; i < unum; i++) {
	    if (bad_user_id(SHM->userid[i]))
		continue;
	    id = SHM->userid[i];
	    adm_give_id_money(id, money, tt);
            fprintf(fp2,"給 %s : %d\n", id, money);
            count++;
	}
        sprintf(buf, "(%d人:%d" MONEYNAME ")", count, count*money);
        strcat(reason, buf);
    } else {
	if (!(fp = fopen("etc/givemoney.txt", "r+"))) {
	    fclose(fp2);
	    return 1;
	}
	while (fgets(buf, sizeof(buf), fp)) {
	    clear();
	    if (!(ptr = strchr(buf, ':')))
		continue;
	    *ptr = '\0';
	    id = buf;
	    mn = ptr + 1;
            money = atoi(mn);
	    adm_give_id_money(id, money, tt);
            fprintf(fp2,"給 %s : %d\n", id, money);
            total_money += money;
            count++;
	}
	fclose(fp);
        sprintf(buf, "(%d人:%d" MONEYNAME ")", count, total_money);
        strcat(reason, buf);

    }

    fclose(fp2);

    sprintf(buf, "%s 紅包機: %s", cuser.userid, reason);
    post_file(BN_SECURITY, buf, "etc/givemoney.log", "[紅包機報告]");
    pressanykey();
    return FULLUPDATE;
}
