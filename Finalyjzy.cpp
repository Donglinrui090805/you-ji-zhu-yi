#include<iostream>
#include<vector>
#include<string>
#include<windows.h>
#include<objidl.h>
#include<gdiplus.h>
#include<opencv2/core.hpp>
#include<opencv2/highgui.hpp>
#include<opencv2/imgcodecs.hpp>
#include<opencv2/imgproc.hpp>
using namespace std;

#define ts_1 0
#define ts_2 30
#define MAX_LINES 10
#define LINE_HEIGHT 24
#define FONT_SIZE_PX 24
#define DIALOG_HEIGHT 240
#define DIALOG_PADDING 16

int a[110],favor[110];
Gdiplus::Font* g_font = nullptr;
Gdiplus::FontFamily* g_fontFamily = nullptr;
cv::Mat g_baseBGRA;
static wstring toW(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static int getTextW(const wstring& text) {
    Gdiplus::Bitmap measureBmp(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics measureG(&measureBmp);
    Gdiplus::RectF bbox;
    measureG.MeasureString(text.c_str(), -1, g_font, Gdiplus::PointF(0, 0), &bbox);
    return (int)bbox.Width;
}
static cv::Size getWinSize(const cv::Mat& frame) {
    HWND hwnd = FindWindowW(nullptr, L"story");
    if (hwnd) {
        RECT rc;
        if (GetClientRect(hwnd, &rc)) {
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            if (w > 0 && h > 0) return cv::Size(w, h);
        }
    }
    try {
        cv::Rect r = cv::getWindowImageRect("story");
        if (r.width > 0 && r.height > 0) return cv::Size(r.width, r.height);
    } catch (...) {}
    return cv::Size(frame.cols, frame.rows);
}

static cv::Size g_lastWinSize(0, 0);
static void showFrame(const cv::Mat& frame) {
    cv::Size ws = getWinSize(frame);
    g_lastWinSize = ws;
    int ww = ws.width, wh = ws.height;
    double s = ((double)ww / frame.cols) < ((double)wh / frame.rows) ? ((double)ww / frame.cols) : ((double)wh / frame.rows);
    int tw = (int)(frame.cols * s);
    int th = (int)(frame.rows * s);
    if (tw <= 0 || th <= 0) { cv::imshow("story", frame); return; }
    cv::Mat scaled;
    cv::resize(frame, scaled, cv::Size(tw, th), 0, 0, cv::INTER_AREA);
    cv::Mat canvas(wh, ww, CV_8UC3, cv::Scalar(0, 0, 0));
    int x = (ww - tw) / 2;
    int y = (wh - th) / 2;
    scaled.copyTo(canvas(cv::Rect(x, y, tw, th)));
    cv::imshow("story", canvas);
}

static void showFrameCached(const cv::Mat& frame) {
    cv::Size ws = getWinSize(frame);
    if (ws == g_lastWinSize) return;
    showFrame(frame);
}

static bool windowAlive() {
    try {
        return cv::getWindowProperty("story", cv::WND_PROP_VISIBLE) > 0;
    } catch (...) {
        return false;
    }
}
static void checkWindow() {
    if (!windowAlive()) exit(0);
}

static void refreshShow(const cv::Mat& show, int totalMs, int stepMs = 16) {
    int waited = 0;
    while (waited < totalMs) {
        showFrameCached(show);
        cv::waitKey(stepMs);
        waited += stepMs;
        checkWindow();
    }
}

//F键检测：waitKey返回值 + GetAsyncKeyState硬件状态兜底（不依赖窗口焦点）
static bool isFavorKey(int key) {
    if (key == 'f' || key == 'F') return true;
    return (GetAsyncKeyState('F') & 0x8000) != 0;
}

//逐字间隔等待；按F返回true，否则等满totalMs返回false
static bool waitCharInterval(const cv::Mat& show, int totalMs, int stepMs = 16) {
    int waited = 0;
    while (waited < totalMs) {
        showFrameCached(show);
        int key = cv::waitKey(stepMs);
        if (isFavorKey(key)) return true;
        waited += stepMs;
        checkWindow();
    }
    return false;
}

//前向声明：好感度查看页面（定义在后面）
void showFavor();

cv::Mat renderUI(const cv::Mat& baseBGRA, const vector<string>& lines, const string& prompt = "") {
    int imgH = baseBGRA.rows;
    int imgW = baseBGRA.cols;
    cv::Mat out = baseBGRA.clone();
    cv::Mat band = out(cv::Rect(0, imgH - DIALOG_HEIGHT, imgW, DIALOG_HEIGHT));
    cv::Mat black(band.size(), CV_8UC4, cv::Scalar(0, 0, 0, 255));
    cv::addWeighted(band, 0.30, black, 0.50, 0, band);
    Gdiplus::Bitmap bitmap(out.cols, out.rows, (INT)out.step, PixelFormat32bppARGB, out.data);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    int y = imgH - DIALOG_HEIGHT + DIALOG_PADDING + FONT_SIZE_PX - 10;
    for (auto& line : lines) {
        if (line.empty()) { y += LINE_HEIGHT; continue; }
        wstring wline = toW(line);
        int w = getTextW(wline);
        int x = (out.cols - w) / 2;
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::PointF pt((float)x, (float)y);
        graphics.DrawString(wline.c_str(), -1, g_font, pt, &brush);
        y += LINE_HEIGHT;
    }
    if (!prompt.empty()) {
        wstring wprompt = toW(prompt);
        int w = getTextW(wprompt);
        int x = (out.cols - w) / 2;
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::PointF pt((float)x, (float)y);
        graphics.DrawString(wprompt.c_str(), -1, g_font, pt, &brush);
    }
    cv::Mat result;
    cv::cvtColor(out, result, cv::COLOR_BGRA2BGR);
    return result;
}

static void waitAnyKey(const vector<string>& buf, const string& prompt = "") {
    cv::Mat show = renderUI(g_baseBGRA, buf, prompt);
    while (true) {
        showFrameCached(show);
        int key = cv::waitKey(16);
        if (isFavorKey(key)) {
            showFavor();
            showFrame(show);
            continue;
        }
        if (key == ' ') break;
        checkWindow();
    }
}

//文字输出
void t_print(cv::Mat& baseImg, vector<string>& buf, const string& s, bool waitEnd = true, const string& promptText = "请输入选择：按 1 / 2") {
    vector<string> chars;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        int len;
        if (c < 0x80) len = 1;
        else if (c < 0xE0) len = 2;
        else if (c < 0xF0) len = 3;
        else len = 4;
        chars.push_back(s.substr(i, len));
        i += len;
    }
    string curLine;
    for (auto& ch : chars) {
        curLine += ch;
        if (buf.empty())
            buf.push_back(curLine);
        else
            buf.back() = curLine;
        cv::Mat show = renderUI(g_baseBGRA, buf, "");
        showFrame(show);
        if (waitCharInterval(show, ts_2)) {
            showFavor();
            showFrame(show);
        }
        checkWindow();
    }
    if (waitEnd) {
        buf.push_back("");
        waitAnyKey(buf);
    } 
    else {
        buf.push_back("");
        cv::Mat show = renderUI(g_baseBGRA, buf, promptText);
        showFrame(show);
    }
}

//双选
int waitSelect(const cv::Mat& baseImg, vector<string>& buf) {
    string prompt = "请输入选择：按 1 / 2";
    cv::Mat show = renderUI(g_baseBGRA, buf, prompt);
    while (true) {
        int key = cv::waitKey(20);
        if (isFavorKey(key)) {
            showFavor();
            showFrame(show);
            continue;
        }
        if (key == '1') return 1;
        if (key == '2') return 2;
        checkWindow();
        showFrameCached(show);
    }
}

//三选
int waitSelect3(const cv::Mat& baseImg, vector<string>& buf) {
    string prompt = "请输入选择：按 1 / 2 / 3";
    cv::Mat show = renderUI(g_baseBGRA, buf, prompt);
    while (true) {
        int key = cv::waitKey(20);
        if (isFavorKey(key)) {
            showFavor();
            showFrame(show);
            continue;
        }
        if (key == '1') return 1;
        if (key == '2') return 2;
        if (key == '3') return 3;
        checkWindow();
        showFrameCached(show);
    }
}

//图片输出
cv::Mat p_print(string s) {
    cv::Mat img = cv::imread(s);
    if (img.empty()) {
        MessageBoxW(nullptr, L"图片加载失败！请确认图片文件与 story.exe 在同一目录。", L"story", MB_OK | MB_ICONERROR);
        return cv::Mat();
    }
    cv::cvtColor(img, g_baseBGRA, cv::COLOR_BGR2BGRA);
    return img;
}

//清屏
void clearScreen(vector<string>& buf) {
    buf.clear();
}

//好感度系统
void updateFavor(int person, int favorValue) {
    favor[person] += favorValue;
}

//查看好感度页面，按空格返回
void showFavor() {
    vector<string> favorLines;
    favorLines.push_back("【好感度】");
    favorLines.push_back("樊叙：" + to_string(favor[1]));
    favorLines.push_back("周翼：" + to_string(favor[2]));
    favorLines.push_back("姜博：" + to_string(favor[3]));
    favorLines.push_back("");
    cv::Mat show = renderUI(g_baseBGRA, favorLines, "按空格返回游戏");
    showFrame(show);
    while (true) {
        showFrameCached(show);
        int key = cv::waitKey(16);
        if (key == ' ') break;
        checkWindow();
    }
    while (GetAsyncKeyState('F') & 0x8000) {
        cv::waitKey(16);
        checkWindow();
    }
}

void showCover(const cv::Mat& img, int seconds) {
    showFrame(img);
    int waited = 0;
    while (waited < seconds * 1000) {
        showFrameCached(img);
        cv::waitKey(16);
        waited += 16;
        checkWindow();
    }
}

int main() {
    Gdiplus::GdiplusStartupInput gdiplusSI;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusSI, nullptr);
    g_fontFamily = new Gdiplus::FontFamily(L"SimHei");
    g_font = new Gdiplus::Font(g_fontFamily, (float)FONT_SIZE_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    cv::Mat img = p_print("image2.png");
    cv::namedWindow("story", cv::WINDOW_NORMAL);
    {
        int scrW = GetSystemMetrics(SM_CXSCREEN);
        int scrH = GetSystemMetrics(SM_CYSCREEN);
        double s = ((double)(scrW * 92 / 100) / img.cols) < ((double)(scrH * 92 / 100) / img.rows) ? ((double)(scrW * 92 / 100) / img.cols) : ((double)(scrH * 92 / 100) / img.rows);
        cv::resizeWindow("story", (int)(img.cols * s), (int)(img.rows * s));
    }
    vector<string> textBuf;
    //封面页：仅图片，停留5秒
    showCover(img, 5);
    //说明页：操作说明，按空格开始游戏
    vector<string> introLines;
    t_print(img, textBuf, "按空格进入下一句对话");
    t_print(img, textBuf, "按F查看当前各角色好感度");
    t_print(img, textBuf, "");
    t_print(img, textBuf, "按空格开始游戏", false);
    cv::waitKey(16);
    clearScreen(textBuf);
    //第一章
    t_print(img, textBuf, "14班成立，班内同学关系不佳。");
    t_print(img, textBuf, "高皓远认为从竞赛班淘汰的樊叙“装”。");
    t_print(img, textBuf, "樊叙看不惯唐颖怼老师，发生口角。");
    clearScreen(textBuf);
    t_print(img, textBuf, "小马在12班讲课被学生刁难。");
    t_print(img, textBuf, "校方不允许竞赛班以外的学生参加化学竞赛。");
    t_print(img, textBuf, "1:14班凭“化学杀”实力说服了化学老师陈阳，最终得到她的支持。");
    t_print(img, textBuf, "2:14班没能成功说服化学老师陈阳。", false);
    a[0] = waitSelect(img, textBuf);
    clearScreen(textBuf);
    if (a[0] == 1) {
        //第二章
        t_print(img, textBuf, "十一假期樊叙参加化学竞赛，周翼离家出走。");
        t_print(img, textBuf, "周翼被找回，接受批评教育。");
        t_print(img, textBuf, "1:进一步指责。");
        t_print(img, textBuf, "2:深入了解具体情况。", false);
        a[1] = waitSelect(img, textBuf);
        clearScreen(textBuf);
        //第三章
        t_print(img, textBuf, "化学竞赛出结果，樊叙、游智慧落榜，姜博入围国赛。");
        t_print(img, textBuf, "实习老师给12班代生物课，有学生拒不配合，认为年轻老师不行。");
        t_print(img, textBuf, "14班为老师出头。");
        t_print(img, textBuf, "1:不作为。");
        t_print(img, textBuf, "2:劝告主谋高皓远下次不要这样。", false);
        a[2] = waitSelect(img, textBuf);
        clearScreen(textBuf);
    }
    t_print(img, textBuf, "14班初次月考成绩不尽人意，小马遭到压力，立下flag要让班级平均达到550。");
    t_print(img, textBuf, "游智慧指出“学历并非唯一标准”。");
    t_print(img, textBuf, "1:觉得她说的对，但是告诉大家“高考也是人生很重要的一步”。");
    t_print(img, textBuf, "2:否认她的观点，使她愤然离去。", false);
    a[3] = waitSelect(img, textBuf);
    clearScreen(textBuf);
    if (a[3] == 1) {
        t_print(img, textBuf, "樊叙成为数学课代表，与之前关系一般的朱李泰共事，解开了对于数学的心理压力。");
        clearScreen(textBuf);
    } 
    else {
        t_print(img, textBuf, "我后来才明白，人的离开是不需要理由的。");
        t_print(img, textBuf, "就像那盆绿萝，我每天浇水，它还是枯了。我把它放在窗台上，想着哪天再买一盆新的，后来就忘了。");
        t_print(img, textBuf, "再后来座位空了，我最后一次回头看了一眼14班的窗户，窗帘拉着，里面什么都没有。");
        t_print(img, textBuf, "我原本以为自己可以改变什么的。");
        clearScreen(textBuf);
        t_print(img, textBuf, "刚来的时候，我写过一张纸条贴在办公桌边上：“没有坏学生，只有还没被看见的学生。”");
        t_print(img, textBuf, "那张纸条贴了很久，边角卷了，字迹也模糊了。");
        t_print(img, textBuf, "走的那天我把它撕下来，折了四折，放进大衣口袋里。");
        t_print(img, textBuf, "后来洗衣服的时候忘了掏出来，它就碎了。");
        clearScreen(textBuf);
        t_print(img, textBuf, "我走的那天是个晴天。");
        t_print(img, textBuf, "阳光特别亮，照着走廊，照着空无一人的教室，照着桌上那盆枯萎的绿萝。");
        t_print(img, textBuf, "我没有回头，也没有哭。我只是一直往前走，走到校门口的时候停了一下，然后继续走。");
        clearScreen(textBuf);
        t_print(img, textBuf, "后来有人问我，后悔吗。");
        t_print(img, textBuf, "我说不后悔。我不是不后悔，我是不能说后悔");
        t_print(img, textBuf, "——如果说后悔，那这一年就真的什么都没剩下。");
        clearScreen(textBuf);
        t_print(img, textBuf, "你问我那一年留下了什么。");
        t_print(img, textBuf, "我留下的不是分数，不是排名，不是高考成绩单上的数字。");
        t_print(img, textBuf, "我留下的是那天阳光落在你脸上的样子,");
        t_print(img, textBuf, "是你第一次叫我“老师”的时候声音还有点抖，");
        t_print(img, textBuf, "是你后来不再躲我的眼神。");
        t_print(img, textBuf, "这些就够了。");
        clearScreen(textBuf);
        t_print(img, textBuf, "我走了之后，有人接了我的班，继续坐在那间办公室里。");
        t_print(img, textBuf, "绿萝被扔掉了，新老师买了新的盆栽放在窗台上，比我的那盆绿。");
        t_print(img, textBuf, "14班的门牌被换掉了。新的学生走进去，新的老师在黑板上写字，新的阳光照进来，新的灰尘落下去。");
        t_print(img, textBuf, "一切都和我来之前一样。");
        t_print(img, textBuf, "一切都和我走之后一样。");
        t_print(img, textBuf, "时间落下来，变成一层很薄很薄的灰。我走过去，留下脚印，然后脚印也被覆盖了。");
        t_print(img, textBuf, "但我记得。我记得你们。");
        clearScreen(textBuf);
        t_print(img, textBuf, "【NE1-和光同尘】");
        t_print(img, textBuf, "按空格结束游戏", false, "");
        waitAnyKey(textBuf);
        cv::destroyAllWindows();
        delete g_font;
        delete g_fontFamily;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }
    //第四章
    t_print(img, textBuf, "姜博为完成社团任务拍摄了微电影，并上传视频软件，主题是“别让过度内卷磨灭了自己的个性”。");
    t_print(img, textBuf, "被教导主任看到，举报给小马和校长。作为导演和主演的姜博受处分风险，14班一齐为他求情。");
    t_print(img, textBuf, "恰逢教育局突击检查素质教育落实情况，校长顺势将事件包装成“学生创新实践案例”，事件最终平息。");
    t_print(img, textBuf, "姜博受到一定打击。");
    clearScreen(textBuf);
    //第五章
    t_print(img, textBuf, "主任：“覃先生，覃太太，你们放心，这件事学校一定会严肃处理，绝对不让咱们优秀的同学受委屈。”");
    t_print(img, textBuf, "覃母：“主任，我们正则是要冲刺清北强基的。现在高三这么关键，被这种不学无术的学生弄伤了手，万一影响考试怎么办？”");
    t_print(img, textBuf, "“这要是记个过就算了，那二中的门槛也太低了点。我建议，这两个学生，全部劝退。”");
    t_print(img, textBuf, "“樊父：“正则妈妈您说得对，是我们家教不严，对不起对不起……”");
    clearScreen(textBuf);
    t_print(img, textBuf, "（樊叙父亲转过身，看着樊叙，突然扬起手——“啪”的一声脆响。）");
    t_print(img, textBuf, "（樊叙被打得偏过头，耳边嗡嗡作响。）");
    t_print(img, textBuf, "樊父：“你还要不要脸？我花那么多钱供你读书，你跑来学校跟人谈恋爱？”");
    t_print(img, textBuf, "“你成绩烂成那样我都没说你，你现在连惹事生非都学会了？你是不是非要把我的脸丢尽才甘心！”");
    t_print(img, textBuf, "樊叙：“……是他先骂我的……”");
    t_print(img, textBuf, "樊父：“他还骂错了？！你看看人家年级第一，你再看看你！你算个什么东西，人家凭什么不能骂你？！”");
    t_print(img, textBuf, "（樊叙父亲暴怒，准备再扇女儿一巴掌。）");
    clearScreen(textBuf);
    t_print(img, textBuf, "（周翼猛地甩开主任的手，一步跨过去，把樊叙挡在身后。樊叙母亲此时也冲了上来。）");
    t_print(img, textBuf, "周翼：“叔叔，您干什么！”");
    t_print(img, textBuf, "樊母：“你疯了吧！在学校呢，你打孩子干什么！”");
    t_print(img, textBuf, "樊父：“看来你就是那个拱我家白菜的猪了。你父母怎么教你的……”");
    clearScreen(textBuf);
    t_print(img, textBuf, "（就在这时，办公室的门开了。周翼的父母来了。）");
    t_print(img, textBuf, "周母：“周翼，你到底惹什么事了？”");
    t_print(img, textBuf, "覃母：“周翼妈妈，你儿子可真是‘有出息’！为了个考试不及格的女生，把年级第一给打了。”");
    t_print(img, textBuf, "周父：“你长本事了吧，为了这么个死丫头跟人打架？你知不知道爸爸为了给你找补习班求了多少人？你对得起我吗？！”");
    t_print(img, textBuf, "周翼：“……”");
    clearScreen(textBuf);
    t_print(img, textBuf, "周父：“跪下来。”");
    t_print(img, textBuf, "（周翼紧咬着下唇，膝盖微曲，在所有家长、老师和同学的注视下，双膝落地，跪了下来。）");
    t_print(img, textBuf, "周父：“道歉。”");
    t_print(img, textBuf, "周翼：“……对不起。”");
    clearScreen(textBuf);
    t_print(img, textBuf, "（樊叙冲过去，想要扶起周翼。）");
    t_print(img, textBuf, "樊叙：“叔叔，让周翼起来吧，他没有错……”");
    t_print(img, textBuf, "（所有人安静了一秒。）");
    t_print(img, textBuf, "樊叙：“他打架是因为我。覃正则骂我，他替我出头。你们要处分就处分我，要开除就开除我！周翼没有错……”");
    clearScreen(textBuf);
    t_print(img, textBuf, "这是你作为班主任，必须出面的时刻。你需要做出选择：");
    t_print(img, textBuf, "1.强硬介入，护住学生");
    t_print(img, textBuf, "2:迂回战术，平息事态");
    t_print(img, textBuf, "3:沉默旁观，无力干预", false, "请输入选择：按 1 / 2 / 3");
    a[4] = waitSelect3(img, textBuf);
    clearScreen(textBuf);
    if (a[4] == 1) {
        t_print(img, textBuf, "“主任，正则爸爸，正则妈妈。事情的起因是覃正则同学在走廊上公开羞辱樊叙同学在先。");
        t_print(img, textBuf, "周翼动手确实违纪，该记过记过，我绝不袒护。但是——这里是学校，不是审判庭。");
        t_print(img, textBuf, "两个孩子我带回班教育。如果谁再动手打我的学生，我会直接报警。”");
    } 
    else if(a[4] == 2) {
        t_print(img, textBuf, "“樊叙爸爸，周翼爸爸，我知道你们急。但你们看看孩子现在，他们很害怕。");
        t_print(img, textBuf, "覃正则他们家那么狂，我们现在硬碰硬只会让孩子们吃亏。");
        t_print(img, textBuf, "给我点时间，我去和主任交涉。在这之前，别当着主任和别人家长的面打孩子，他们会看笑话的。”");
    } 
    else {
        t_print(img, textBuf, "（小马站在旁边，无力地看着三家人对峙。她知道“坏学生”带来的偏见是一座大山，她搬不动。）");
        clearScreen(textBuf);
        t_print(img, textBuf, "（周翼父亲把樊叙推开，揪住周翼的头发）");
        t_print(img, textBuf, " 周翼：“……”");
        t_print(img, textBuf, "（周围充满了众人的目光，以及议论，以及惊恐。）");
        t_print(img, textBuf, "（没有一个人为他说话。就连他心爱的女孩，也只是在一旁无力地落泪。）");
        clearScreen(textBuf);
        t_print(img, textBuf, "我又梦到了你。");
        t_print(img, textBuf, "——你是谁？");
        t_print(img, textBuf, "为什么……总出现在我的梦里？");
        t_print(img, textBuf, "我不认识你。不，我应该认识你。");
        t_print(img, textBuf, "我记得你掌心的温度。");
        t_print(img, textBuf, "那双曾牵过我的手……多么，多么炽热。");
        clearScreen(textBuf);
        t_print(img, textBuf, "牵好我。");
        t_print(img, textBuf, "抓紧我。");
        t_print(img, textBuf, "别走。");
        t_print(img, textBuf, "求你留在我身边。");
        t_print(img, textBuf, "可是……");
        t_print(img, textBuf, "可是我已经不记得你的脸了。");
        clearScreen(textBuf);
        t_print(img, textBuf, "求你不要再出现在我的梦里了……");
        t_print(img, textBuf, "求你了。求你了。求你了。求你了。求你了。求你了。求你了。");
        t_print(img, textBuf, "你去看看别人吧。");
        t_print(img, textBuf, "一定会有人记得你……一定会有人比我更想念你的。");
        clearScreen(textBuf);
        t_print(img, textBuf, "【BE2-光谱红移】");
        t_print(img, textBuf, "“宇宙在膨胀，星辰在远离。”");
        t_print(img, textBuf, "“我们都在坠落，只是隔着光年，再也无法触及彼此。”");
        t_print(img, textBuf, "按空格结束游戏", false, "");
        waitAnyKey(textBuf);
        cv::destroyAllWindows();
        delete g_font;
        delete g_fontFamily;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }
    clearScreen(textBuf);
    t_print(img, textBuf, "小马和12班班主任赵娟算是结下梁子了，赵娟还在办公室贬低过樊叙，被小马反驳。");
    clearScreen(textBuf);
    //第六章
    t_print(img, textBuf, "小马过年期间回家，被催婚，且家里人都不太看好她的工作。");
    clearScreen(textBuf);
    t_print(img, textBuf, "辩论大会，题目“优绩主义究竟是社会进步的动力，还是加剧不公与焦虑的枷锁”。");
    t_print(img, textBuf, "高皓远带领14班同学反抗强权压制。");
    clearScreen(textBuf);
    //第七章
    t_print(img, textBuf, "临近模考，赵娟把小马因学术纠纷肄业的事情抖落出来，小马被12班家长联名抵制。");
    t_print(img, textBuf, "但是由于14班同学的坚持，她继续担任14班班主任。");
    //最终章
    t_print(img, textBuf, "高考当天，小马心情跌宕起伏。");
    t_print(img, textBuf, "按空格结束游戏", false, "");
    waitAnyKey(textBuf);
    cv::destroyAllWindows();
    delete g_font;
    delete g_fontFamily;
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}
