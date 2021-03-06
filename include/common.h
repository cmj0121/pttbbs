/* $Id$ */
#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#define STR_GUEST	"guest"	    // guest account
#define STR_REGNEW	"new"	    // 用來建新帳號的名稱
#ifdef USE_VERIFYDB_ACCOUNT_RECOVERY
# define STR_RECOVER	"/recover"  // recover function
#endif

#define DEFAULT_BOARD   str_sysop

// BBS Configuration Files
#define FN_CONF_EDITABLE	"etc/editable"	    // 蝡������舐楊頛舐��蝟餌絞瑼�獢����銵�
#define FN_CONF_RESERVED_ID	"etc/reserved.id"   // 靽����蝟餌絞��函�⊥��閮餃����� ID
#define FN_CONF_BINDPORTS	"etc/bindports.conf"   // ���閮剛�����靘����蝺���������� port ���銵�
#define FN_CONF_BANIP           BBSHOME "/etc/banip.conf"    // 蝳�甇ａ��蝺���� IP ���銵�

// BBS Data File Names
#define FN_PASSWD       BBSHOME "/.PASSWDS"      /* User records */
#define FN_CHICKEN	"chicken"
#define FN_USSONG       "ussong"        /* 暺�甇�蝯梯�� */
#define FN_POST_NOTE    "post.note"     /* po���蝡����敹���� */
#define FN_POST_BID     "post.bid"
#define FN_MONEY        "etc/money"
#define FN_OVERRIDES    "overrides"
#define FN_REJECT       "reject"
#define FN_WATER        "water"         // ���瘞湔▲
#define FN_BANNED       "banned"        // ��唳偌獢�
#define FN_BANNED_HISTORY "banned.history"  // ��唳偌獢嗡��甇瑕�脰�����
#define FN_BADPOST_HISTORY "badpost.history"  // ������甇瑕�脰�����
#define FN_CANVOTE      "can_vote"
#define FN_VISABLE      "visable"	// 銝���仿����航狐��潮�舐��嚗�撠���臬停��臬��...
#define FN_ALOHAED      "alohaed"       // 銝�蝡�閬������交����������� (蝺刻摩���)
#define FN_ALOHA        "aloha"         // ���銝�蝡�閬������亦�������� (��芸����Ｙ��)
#define FN_USIES        "usies"         /* BBS log */
#define FN_DIR		".DIR"
#define FN_DIR_BOTTOM   ".DIR.bottom"
#define FN_BOARD        ".BRD"          /* board list */
#define FN_USEBOARD     "usboard"       /* �����輻絞閮� */
#define FN_TOPSONG      "etc/topsong"
#define FN_OVERRIDES    "overrides"
#define FN_TICKET       "ticket"
#define FN_TICKET_END   "ticket.end"
#define FN_TICKET_LOCK  "ticket.end.lock"
#define FN_TICKET_ITEMS "ticket.items"
#define FN_TICKET_RECORD "ticket.data"
#define FN_TICKET_USER    "ticket.user"  
#define FN_TICKET_OUTCOME "ticket.outcome"
#define FN_TICKET_BRDLIST "boardlist"
#define FN_BRDLISTHELP	"etc/boardlist.help"
#define FN_BOARDHELP	"etc/board.help"
#define FN_MAIL_ACCOUNT_SYSOP "etc/mail_account_sysop"  // 撣唾��蝡���瑚縑蝞勗��銵�
#define FN_MAIL_ACCOUNT_SYSOP_DESC "etc/mail_account_sysop_desc"  // 撣唾��蝡���瑚縑蝞梯牧���
#define FN_USERMEMO	"memo.txt"	// 雿輻�刻�����鈭箄��鈭����
#define FN_BADLOGIN	"logins.bad"	// in BBSHOME & user directory
#define FN_RECENTLOGIN	"logins.recent"	// in user directory
#define FN_FORWARD      ".forward"      /* auto forward */
#ifndef SZ_RECENTLOGIN
#define SZ_RECENTLOGIN	(16000)		// size of max recent log before rotation
#endif
#define FN_RECENTPAY    "money.recent"
#ifndef SZ_RECENTPAY
#define SZ_RECENTPAY    (16000)
#endif

// ��芾����芷�斗��蝡������箇�曄��璅�憿����瑼�獢�
#ifndef FN_SAFEDEL
#ifdef USE_EDIT_HISTORY
// For edit_history, the new file name must be as short as possible so that so we can restore it later.
#define FN_SAFEDEL	".d"
#define FN_SAFEDEL_PREFIX_LEN (2) // must match FN_SAFEDEL
#else
#define FN_SAFEDEL	".deleted"
#endif // USE_EDIT_HISTORY
#endif // FN_SAFEDEL
#ifndef STR_SAFEDEL_TITLE
#define STR_SAFEDEL_TITLE   "(��祆��撌脰◤��芷��)"
#endif 
#define FN_EDITHISTORY  ".history"

#ifndef SAFE_ARTICLE_DELETE_NUSER
#define SAFE_ARTICLE_DELETE_NUSER (2)
#endif

#define MSG_DEL_CANCEL  "���瘨���芷��"
#define MSG_BIG_BOY     "�����臬之撣亙��! ^o^Y"
#define MSG_BIG_GIRL    "銝�蝝�憭抒��憟� *^-^*"
#define MSG_LITTLE_BOY  "�����臬��餈芸��... =)"
#define MSG_LITTLE_GIRL "�����舀�����蝢����! :>"
#define MSG_MAN         "暻亦�嗅�������� (^O^)"
#define MSG_WOMAN       "��急��撠���踹夾!! /:>"
#define MSG_PLANT       "璊���拐�������批�亙��.."
#define MSG_MIME        "蝷衣�拍蜇瘝���批�乩�����"

#define MSG_CLOAKED     "撌脤�脣�仿�勗耦璅∪��(銝������潔蝙��刻�������桐��)"
#define MSG_UNCLOAK     "撌脤�ａ����勗耦璅∪��(��祇����潔蝙��刻�������桐��)"

#define MSG_WORKING     "������銝哨��隢�蝔����..."

#define MSG_CANCEL      "���瘨����"
#define MSG_USR_LEFT    "雿輻�刻��撌脩����ａ��鈭�"
#define MSG_NOBODY      "��桀����∩犖銝�蝺�"

#define MSG_DEL_OK      "��芷�文�����"
#define MSG_DEL_CANCEL  "���瘨���芷��"
#define MSG_DEL_ERROR   "��芷�日�航炊"
#define MSG_DEL_NY      "隢�蝣箏����芷��(Y/N)?[N] "

#define MSG_FWD_OK      "���蝡�頧�撖�摰����!"
#define MSG_FWD_ERR1    "頧�撖���航炊: 蝟餌絞��航炊 system error"
#define MSG_FWD_ERR2    "頧�撖���航炊: 雿������航炊 address error"

#define MSG_SURE_NY     "隢���函Ⅱ摰�(Y/N)嚗�[N] "
#define MSG_SURE_YN     "隢���函Ⅱ摰�(Y/N)嚗�[Y] "

#define MSG_BID         "隢�頛詨�亦����踹��蝔�: "
#define MSG_UID         "隢�頛詨�乩蝙��刻��隞����: "
#define MSG_PASSWD      "隢�頛詨�交�函��撖�蝣�: "

#define MSG_BIG_BOY     "�����臬之撣亙��! ^o^Y"
#define MSG_BIG_GIRL    "銝�蝝�憭抒��憟� *^-^*"
#define MSG_LITTLE_BOY  "�����臬��餈芸��... =)"
#define MSG_LITTLE_GIRL "�����舀�����蝢����! :>"
#define MSG_MAN         "暻亦�嗅�������� (^O^)"
#define MSG_WOMAN       "��急��撠���踹夾!! /:>"
#define MSG_PLANT       "璊���拐�������批�亙��.."
#define MSG_MIME        "蝷衣�拍蜇瘝���批�乩�����"

#define ERR_BOARD_OPEN  ".BOARD ��������航炊"
#define ERR_BOARD_UPDATE        ".BOARD ��湔�唳��隤�"
#define ERR_PASSWD_OPEN ".PASSWDS ��������航炊"

#define ERR_BID         "雿������臭����佗��瘝������������踹��嚗�"
#define ERR_UID         "���鋆⊥�����������鈭箏�佗��"
#define ERR_PASSWD      "撖�蝣潔��撠����嚗�隢�瑼Ｘ�亙董������撖�蝣澆之撠�撖急����∟撓��仿�航炊���"
#define ERR_FILENAME    "瑼����銝�甇�蝣綽��"

#define TN_ANNOUNCE	"[��砍��]"

#define STR_AUTHOR1     "雿����:"
#define STR_AUTHOR2     "��潔縑鈭�:"
#define STR_POST1       "������:"
#define STR_POST2       "蝡����:"

#define STR_LOGINDAYS	"��餃�交活���"
#define STR_LOGINDAYS_QTY "甈�"

/* AIDS */
#define AID_DISPLAYNAME	"���蝡�隞�蝣�(AID)"
/* end of AIDS */

/* QUERY_ARTICLE_URL */
#define URL_DISPLAYNAME "���蝡�蝬脣��"
/* end of QUERY_ARTICLE_URL */

/* LONG MESSAGES */
#define MSG_SELECT_BOARD ANSI_COLOR(7) "��� ��豢�������� ���" ANSI_RESET "\n" \
			"隢�頛詨�亦����踹��蝔�(���蝛箇�賡�菔�芸�����撠�): "

#define MSG_SEPARATOR \
"���������������������������������������������������������������������������������������������������������������������"

/* Flags to getdata input function */
#define NOECHO       0
#define DOECHO       1
#define LCECHO       2
#define NUMECHO	     4
#define GCARRY	     8	// (from M3) do not empty input buffer. 
#define PASSECHO    0x10 

#define YEA  1		       /* Booleans  (Yep, for true and false) */
#define NA   0

#define EQUSTR 0	/* for strcmp */

/* 憟賢�����靽� */
#define IRH 1   /* I reject him.		*/
#define HRM 2   /* He reject me.		*/
#define IBH 4   /* I am board friend of him.	*/
#define IFH 8   /* I friend him (He is one of my friends). */
#define HFM 16  /* He friends me (I am one of his friends). */
#define ST_FRIEND  (IBH | IFH | HFM)
#define ST_REJECT  (IRH | HRM)       

#define QCAST           int (*)(const void *, const void *)
#define chartoupper(c)  ((c >= 'a' && c <= 'z') ? c+'A'-'a' : c)

#define LEN_AUTHOR1     5
#define LEN_AUTHOR2     7

/* the title of article will be truncate to PROPER_TITLE_LEN */
#define PROPER_TITLE_LEN	42


/* ----------------------------------------------------- */
/* 瘞渡��璅∪�� ������摰�蝢�                                     */
/* ----------------------------------------------------- */
#define WB_OFO_USER_TOP		7
#define WB_OFO_USER_BOTTOM	11
#define WB_OFO_USER_NUM		((WB_OFO_USER_BOTTOM) - (WB_OFO_USER_TOP) + 1)
#define WB_OFO_USER_LEFT	28
#define WB_OFO_MSG_TOP		15
#define WB_OFO_MSG_BOTTOM	22
#define WB_OFO_MSG_LEFT		4

/* ----------------------------------------------------- */
/* 璅�憿�憿�敶�                                              */
/* ----------------------------------------------------- */
#define SUBJECT_NORMAL      0
#define SUBJECT_REPLY       1
#define SUBJECT_FORWARD     2
#define SUBJECT_LOCKED      3

/* ----------------------------------------------------- */
/* 蝢斤�������格芋撘�   Ptt                                    */
/* ----------------------------------------------------- */
#define FRIEND_OVERRIDE 0
#define FRIEND_REJECT   1
#define FRIEND_ALOHA    2
// #define FRIEND_POST     3	    // deprecated
#define FRIEND_SPECIAL  4
#define FRIEND_CANVOTE  5
#define BOARD_WATER     6
#define BOARD_VISABLE   7 

#define LOCK_THIS   1    // lock���蝺�銝���賡��銴����
#define LOCK_MULTI  2    // lock������蝺�銝���賡��銴����   

#define MAX_MODES	(127)
#define MAX_RECOMMENDS  (100)

#define STR_CURSOR      ">"
#define STR_CURSOR2     "���"
#define STR_UNCUR       " "
#define STR_UNCUR2      "  "

#define NOTREPLYING     -1
#define REPLYING        0
#define RECVINREPLYING  1

/* ----------------------------------------------------- */
/* 蝺刻摩��券�賊��                                            */
/* ----------------------------------------------------- */
#define EDITFLAG_TEXTONLY   (0x00000001)
#define EDITFLAG_UPLOAD	    (0x00000002)
#define EDITFLAG_ALLOWLARGE (0x00000004)
#define EDITFLAG_ALLOWTITLE (0x00000008)
// #define EDITFLAG_ANONYMOUS  (0x00000010)
#define EDITFLAG_KIND_NEWPOST	(0x00000010)
#define EDITFLAG_KIND_REPLYPOST	(0x00000020)
#define EDITFLAG_KIND_SENDMAIL	(0x00000040)
#define EDITFLAG_KIND_MAILLIST	(0x00000080)
#define EDITFLAG_WARN_NOSELFDEL	(0x00000100)
// #define EDITFLAG_ALLOW_LOCAL    (0x00000200)
#define EDIT_ABORTED	-1

/* ----------------------------------------------------- */
/* ���憭拙恕撣豢�� (xchatd)                                   */
/* ----------------------------------------------------- */
#define EXIT_LOGOUT     0
#define EXIT_LOSTCONN   -1
#define EXIT_CLIERROR   -2
#define EXIT_TIMEDOUT   -3
#define EXIT_KICK       -4

#define CHAT_LOGIN_OK       "OK"
#define CHAT_LOGIN_EXISTS   "EX"
#define CHAT_LOGIN_INVALID  "IN"
#define CHAT_LOGIN_BOGUS    "BG"

/* ----------------------------------------------------- */
/* Grayout Levels                                        */
/* ----------------------------------------------------- */
#define GRAYOUT_COLORBOLD (-2)
#define GRAYOUT_BOLD (-1)
#define GRAYOUT_DARK (0)
#define GRAYOUT_NORM (1)
#define GRAYOUT_COLORNORM (+2)

/* Typeahead */
#define TYPEAHEAD_NONE	(-1)
#define TYPEAHEAD_STDIN	(0)

/* ----------------------------------------------------- */
/* Constants                                             */
/* ----------------------------------------------------- */
#define DAY_SECONDS	    (86400)
#define MONTH_SECONDS	    (DAY_SECONDS*30)
#define MILLISECONDS	    (1000)  // milliseconds of one second

#ifndef SHM_HUGETLB
#define SHM_HUGETLB	04000	/* segment is mapped via hugetlb */
#endif

/* ----------------------------------------------------- */
/* Macros                                                */
/* ----------------------------------------------------- */

#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
    #define __builtin_expect(exp,c) (exp)
#endif

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#ifndef MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define	MAX(a,b)	(((a)>(b))?(a):(b))
#endif

#define toSTR(x)	__toSTR(x)
#define __toSTR(x)	#x
#define char_lower(c)  ((c >= 'A' && c <= 'Z') ? c|32 : c)

#define LOG_IF(x, y)    { if ((x)) { y; } else {} }


#endif
