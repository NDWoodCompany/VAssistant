#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include "app.h"

using namespace Gdiplus;

static const wchar_t* APP_ID = L"bbkim.VAssistant";
static const wchar_t* CLASS_NAME = L"VAssistantWin32";
HWND gWnd = nullptr;
static ULONG_PTR gGdi = 0;

static int gPage = 0;
static bool gLangVi = true;
static bool gMax = false;
static bool gDragging = false;
static POINT gDrag = {};
static bool gResizing = false;
static UINT gResizeHit = 0;
static RECT gResizeStart = {};
static POINT gResizePoint = {};
static int gSelected = 0;
static bool gSelectAll = false;
static bool gDark = true;
static int gScrollY = 0;
static int gContentHeight = 0;
static std::unordered_map<std::wstring, std::unique_ptr<Image>> gImages;
static std::wstring gStatus = L"Sẵn sàng";
static bool gStatusError = false;

static Image* img(const std::wstring& rel) {
    auto i = gImages.find(rel);
    if (i != gImages.end()) return i->second.get();
    auto q = std::make_unique<Image>((ExeDir() + L"\\" + rel).c_str());
    if (q->GetLastStatus() != Ok) return nullptr;
    Image* r = q.get();
    gImages.emplace(rel, std::move(q));
    return r;
}

static Color WHITE(){return Color(255,255,255,255);}
static Color BG(){return gDark?Color(255,11,14,18):Color(255,246,248,251);}
static Color CHROME(){return gDark?Color(255,16,20,26):Color(255,255,255,255);}
static Color PANEL(){return gDark?Color(255,20,25,32):Color(255,255,255,255);}
static Color INPUTC(){return gDark?Color(255,13,17,22):Color(255,247,249,252);}
static Color BORDER(){return gDark?Color(255,37,44,52):Color(255,215,220,226);}
static Color BORDER2(){return gDark?Color(255,34,40,48):Color(255,224,228,233);}
static Color TEXTC(){return gDark?Color(255,231,235,239):Color(255,32,37,43);}
static Color TEXT2(){return gDark?Color(255,210,216,222):Color(255,62,68,76);}
static Color MUTED(){return gDark?Color(255,105,113,123):Color(255,105,113,123);}
static Color SOFT(){return gDark?Color(255,115,123,133):Color(255,115,123,133);}
static Color ACCENT(){return gDark?Color(255,201,47,69):Color(255,201,47,69);}
static Color ACCENT_DARK(){return gDark?Color(255,164,40,58):Color(255,235,181,25);}
static Color GREEN(){return Color(255,53,196,106);}
static Color SELECTED_BG(){return gDark?Color(255,36,43,52):Color(255,224,229,235);}
static Color SELECTED_BORDER(){return gDark?Color(255,61,69,80):Color(255,184,191,200);}
static Color BUTTON_BG(){return gDark?Color(255,28,34,42):Color(255,232,236,241);}
static Color BUTTON_BORDER(){return gDark?Color(255,48,56,66):Color(255,201,207,214);}
static Color BUTTON_TEXT(){return gDark?Color(255,210,216,222):Color(255,45,51,58);}
static void drawDownloadButton(Graphics& g, int x, int y, int w, const std::wstring& normal);

static void roundPath(GraphicsPath&p,const RectF&r,float rad){float d=rad*2;p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);p.AddArc(r.X,r.GetBottom()-d,d,d,90,90);p.CloseFigure();}
static void fillRound(Graphics&g,const RectF&r,Color c,float rad=7){GraphicsPath p;roundPath(p,r,rad);SolidBrush b(c);g.FillPath(&b,&p);}static void strokeRound(Graphics&g,const RectF&r,Color c,float rad=7){GraphicsPath p;roundPath(p,r,rad);Pen q(c,1);g.DrawPath(&q,&p);}
static void text(Graphics&g,const std::wstring&s,float x,float y,float w,float h,float size,Color c,bool bold=false,StringAlignment a=StringAlignmentNear){FontFamily ff(L"Segoe UI");Font f(&ff,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush b(c);StringFormat sf;sf.SetAlignment(a);sf.SetLineAlignment(StringAlignmentCenter);g.DrawString(s.c_str(),-1,&f,RectF(x,y,w,h),&sf,&b);}
static void line(Graphics&g,float x1,float y1,float x2,float y2,Color c){Pen p(c,1);g.DrawLine(&p,x1,y1,x2,y2);}static std::wstring label(const Item&x){return Utf8ToWide(gLangVi?x.vi:x.en);}static void drawImage(Graphics&g,Image*i,const RectF&r){if(i)g.DrawImage(i,r);}
static void check(Graphics&g,float x,float y,bool on){RectF r(x,y,14,14);Color fill=on?SELECTED_BG():INPUTC();Color border=on?SELECTED_BORDER():BORDER();fillRound(g,r,fill,3);strokeRound(g,r,border,3);if(on){Pen p(gDark?WHITE():Color(255,45,51,58),2);g.DrawLine(&p,x+3,y+7,x+6,y+10);g.DrawLine(&p,x+6,y+10,x+11,y+4);}}
static void titlebar(Graphics&g,int w){fillRound(g,RectF(0,0,(float)w,52),CHROME(),0);drawImage(g,img(gDark?L"assets/VAssistant-dark.png":L"assets/VAssistant.png"),RectF(12,11,30,30));text(g,L"VAssistant",51,0,100,52,15,TEXTC(),true);drawImage(g,img(gLangVi?L"assets/vn.png":L"assets/gb.png"),RectF((float)w-157,18,18,12));text(g,gLangVi?L"Vi":L"En",w-135,0,28,52,11,SOFT());text(g,L"—",w-105,0,30,52,14,SOFT(),false,StringAlignmentCenter);text(g,gMax?L"❐":L"□",w-73,0,30,52,12,SOFT(),false,StringAlignmentCenter);text(g,L"×",w-40,0,32,52,16,SOFT(),false,StringAlignmentCenter);line(g,0,51,w,51,BORDER2());}
static const wchar_t*NV[]={L"Cài đặt",L"Công cụ",L"Office",L"Máy in",L"Bloatware",L"OEM",L"Thông tin"};static const wchar_t*NE[]={L"Setup",L"Portable",L"Office",L"Printer",L"Bloatware",L"OEM",L"About"};static const wchar_t*NI[]={L"⚙",L"▣",L"▦",L"▤",L"✚",L"◆",L"ⓘ"};
static void sidebar(Graphics&g,int h){fillRound(g,RectF(0,52,210,(float)h-52),CHROME(),0);text(g,gLangVi?L"CHỨC NĂNG":L"FUNCTIONS",19,65,170,22,10,MUTED());for(int i=0;i<7;i++){float y=92+i*45;bool a=i==gPage;fillRound(g,RectF(10,y,190,42),a?SELECTED_BG():CHROME(),7);strokeRound(g,RectF(10,y,190,42),a?SELECTED_BORDER():Color(0,0,0,0),7);text(g,NI[i],19,y,23,42,15,a?TEXTC():SOFT(),false,StringAlignmentCenter);text(g,NV[i],47,y+2,110,20,12,a?TEXTC():SOFT());text(g,NE[i],47,y+20,110,15,9,a?MUTED():MUTED());}
Color statusColor = gStatusError ? Color(255,235,85,95) : GREEN();
text(g,L"●",18,h-38,12,16,10,statusColor);
text(g,gStatus.empty() ? (gLangVi?L"Sẵn sàng":L"Ready") : gStatus,34,h-40,165,20,9,statusColor);
line(g,209,52,209,h,BORDER2());}
void GuiSetStatus(const std::wstring& s, bool error){
    gStatus = s;
    gStatusError = error;
    if (gWnd) InvalidateRect(gWnd, nullptr, FALSE);
}
static void header(Graphics&g,const std::wstring&a,const std::wstring&b){text(g,a,235,79,800,34,23,TEXTC(),true);text(g,b,235,111,850,25,11,MUTED());}
static int gridColumns(int available, int minCard, int gap, int maxCols){
    if(available <= 0) return 1;
    const int maxCard = 185;
    for(int cols=maxCols; cols>=1; --cols){
        const int needed = cols * maxCard + (cols - 1) * gap;
        if(available >= needed) return cols;
    }
    return 1;
}
static int gridCardWidth(int available, int cols, int gap){
    if(cols < 1) cols = 1;
    const int maxCard = 185;
    int w = (available - (cols - 1) * gap) / cols;
    if(w > maxCard) w = maxCard;
    return std::max(1, w);
}
static void toolbar(Graphics&g){
    RECT rc; GetClientRect(gWnd,&rc);
    const int left=235, right=rc.right-12, y=134, h=38;
    const int refreshW=68, installW=120, gap=8;
    const int totalW=right-left;
    fillRound(g,RectF((float)left,(float)y,(float)totalW,(float)h),PANEL(),8);
    strokeRound(g,RectF((float)left,(float)y,(float)totalW,(float)h),BORDER(),8);
    check(g,(float)left+12,(float)y+12,gSelectAll);
    text(g,gLangVi?L"Chọn tất cả":L"Select all",left+34,y,100,h,11,TEXT2());
    text(g,std::to_wstring(gSelected),right-installW-refreshW-gap-16,y,45,h,10,MUTED(),false,StringAlignmentFar);
    const int refreshX=right-refreshW;
    const int installX=refreshX-gap-installW;
    drawDownloadButton(g,installX,y+6,installW,gLangVi?L"Cài đặt đã chọn":L"Install selected");
    fillRound(g,RectF((float)refreshX,(float)y+6,(float)refreshW,26),BORDER(),6);
    text(g,gLangVi?L"Làm mới":L"Refresh",refreshX,y+6,refreshW,26,10,TEXT2(),false,StringAlignmentCenter);
}

static void setup(Graphics&g){
    RECT rc; GetClientRect(gWnd,&rc);
    header(g,gLangVi?L"Cài đặt":L"Setup",gLangVi?L"Chọn phần mềm muốn cài đặt.":L"Select software to install.");
    toolbar(g);
    const int left=235, right=rc.right-12, gap=8, minCard=155, cardH=46;
    const int cols=gridColumns(right-left,minCard,gap,4);
    const int cardW=gridCardWidth(right-left,cols,gap);
    const int top=182;
    gContentHeight = gSoftware.empty()?0:((int)((gSoftware.size()+cols-1)/cols)*(cardH+gap)-gap);
    GraphicsState clipState = g.Save();
    g.SetClip(RectF((float)left,178.0f,(float)(right-left),std::max<long>(0L,rc.bottom-178L)),CombineModeReplace);
    for(size_t i=0;i<gSoftware.size();++i){
        int col=(int)(i%cols), row=(int)(i/cols);
        float x=(float)(left+col*(cardW+gap));
        float y=(float)(top+row*(cardH+gap)-gScrollY);
        if(y+cardH<178 || y>rc.bottom) continue;
        fillRound(g,RectF(x,y,(float)cardW,(float)cardH),gSoftChecked[i]?SELECTED_BG():PANEL(),7);
        strokeRound(g,RectF(x,y,(float)cardW,(float)cardH),gSoftChecked[i]?SELECTED_BORDER():BORDER(),7);
        check(g,x+8,y+16,gSoftChecked[i]);
        fillRound(g,RectF(x+29,y+11,24,24),BORDER(),4);
        drawImage(g,img(Utf8ToWide(gSoftware[i].icon)),RectF(x+29,y+11,24,24));
        text(g,label(gSoftware[i]),x+61,y,(float)cardW-69,cardH,11,TEXT2());
    }
    g.Restore(clipState);
}
static void portable(Graphics&g){
    RECT rc; GetClientRect(gWnd,&rc);
    header(g,gLangVi?L"Công cụ":L"Portable",gLangVi?L"Chạy các công cụ portable trực tiếp.":L"Run portable tools directly.");
    const int left=235, right=rc.right-12, gap=8, minCard=155, cardH=46;
    const int cols=gridColumns(right-left,minCard,gap,4);
    const int cardW=gridCardWidth(right-left,cols,gap);
    const int top=145;
    gContentHeight = gPortable.empty()?0:((int)((gPortable.size()+cols-1)/cols)*(cardH+gap)-gap);
    GraphicsState clipState = g.Save();
    g.SetClip(RectF((float)left,140.0f,(float)(right-left),std::max<long>(0L,rc.bottom-140L)),CombineModeReplace);
    for(size_t i=0;i<gPortable.size();++i){
        int col=(int)(i%cols), row=(int)(i/cols);
        float x=(float)(left+col*(cardW+gap));
        float y=(float)(top+row*(cardH+gap)-gScrollY);
        if(y+cardH<140 || y>rc.bottom) continue;
        fillRound(g,RectF(x,y,(float)cardW,(float)cardH),PANEL(),7);
        strokeRound(g,RectF(x,y,(float)cardW,(float)cardH),BORDER(),7);
        fillRound(g,RectF(x+8,y+11,24,24),BORDER(),4);
        drawImage(g,img(Utf8ToWide(gPortable[i].icon)),RectF(x+8,y+11,24,24));
        text(g,label(gPortable[i]),x+40,y,(float)cardW-48,cardH,11,TEXT2());
    }
    g.Restore(clipState);
}
static void printer(Graphics&g){header(g,gLangVi?L"Máy in":L"Printer",gLangVi?L"Chọn lỗi thường gặp để xử lý nhanh.":L"Choose a common printer problem to fix.");for(size_t i=0;i<gPrinter.size();i++){float x=235+(float)(i%2)*430,y=145+(float)(i/2)*61;fillRound(g,RectF(x,y,420,54),PANEL(),7);strokeRound(g,RectF(x,y,420,54),BORDER(),7);text(g,Utf8ToWide(gPrinter[i].icon),x+12,y,28,54,15,SOFT(),false,StringAlignmentCenter);text(g,label(gPrinter[i]),x+48,y,350,54,11,TEXT2());}}
static void bloat(Graphics&g){
    RECT rc; GetClientRect(gWnd,&rc);
    header(g,L"Bloatware",gLangVi?L"Chọn thành phần không cần thiết để gỡ.":L"Select unnecessary components to remove.");
    const int left=235,right=rc.right-12,gap=8,minCard=210;
    const int cols=gridColumns(right-left,minCard,gap,4);
    const int cardW=gridCardWidth(right-left,cols,gap);
    float y=145-(float)gScrollY; std::string last;
    for(size_t i=0;i<gBloat.size();){
        if(gBloat[i].group!=last){
            last=gBloat[i].group;
            text(g,Utf8ToWide(last),left,y,800,24,12,TEXT2(),true);
            y+=29;
        }
        size_t j=i; while(j<gBloat.size()&&gBloat[j].group==last) ++j;
        int count=(int)(j-i);
        for(int n=0;n<count;++n){
            float x=(float)(left+(n%cols)*(cardW+gap));
            float yy=y+(float)((n/cols)*46);
            if(yy+39>=140 && yy<=rc.bottom){
                fillRound(g,RectF(x,yy,(float)cardW,39),gBloatChecked[i+n]?SELECTED_BG():PANEL(),6);
                strokeRound(g,RectF(x,yy,(float)cardW,39),gBloatChecked[i+n]?SELECTED_BORDER():BORDER(),6);
                check(g,x+9,yy+12,gBloatChecked[i+n]);
                text(g,label(gBloat[i+n]),x+31,yy,(float)cardW-40,39,10,TEXT2());
            }
        }
        y+=((count+cols-1)/cols)*46+17; i=j;
    }
    gContentHeight=(int)(y+gScrollY-145);
}

static std::wstring gOfficeVersion = L"365";
static std::wstring gOfficeArch = L"x64";
static std::wstring gOfficeEdition = L"ProPlus";
static std::wstring gOfficeChannel = L"SemiAnnual";
static std::vector<std::wstring> gOfficeLangs = {L"en-us"};
static std::vector<std::wstring> gOfficeApps = {L"Word",L"Excel",L"Access",L"PowerPoint",L"Outlook",L"OneNote",L"Publisher",L"Teams"};
static int gOfficeOpen = -1;
static bool gOfficeSettingsOpen = false;
static int gOfficeSettingsTab = 0;
static bool gOfficeAlwaysNew = true;
static bool gOfficeShowWord = true;
static bool gOfficeRuler = true;
static bool gOfficeNormalTemplate = true;
static bool gOfficeProtectedView = true;
static bool gOfficeStartScreen = true;

static std::vector<std::wstring> officeVersions(){return {L"2016",L"2019",L"2021",L"2024",L"365"};}
static std::vector<std::wstring> officeArchs(){return {L"x86",L"x64"};}
static std::vector<std::wstring> officeEditions(){
    if(gOfficeVersion==L"2016") return {L"ProPlus",L"Professional",L"Home & Business",L"Home & Student"};
    if(gOfficeVersion==L"2019"||gOfficeVersion==L"2021") return {L"ProPlus",L"Professional",L"Home & Business",L"Home & Student",L"Standard"};
    if(gOfficeVersion==L"2024") return {L"ProPlus",L"Professional",L"Home & Business",L"Home",L"Standard"};
    return {L"ProPlus",L"Business"};
}
static std::vector<std::wstring> officeChannels(){
    if(gOfficeVersion==L"365") return {L"SemiAnnual",L"SemiAnnualPreview",L"MonthlyEnterprise",L"Current",L"CurrentPreview",L"BetaChannel"};
    return {L"Perpetual"};
}
static const wchar_t* officeChannelText(const std::wstring& s){
    if(s==L"SemiAnnual") return L"Semi-Annual";
    if(s==L"SemiAnnualPreview") return L"Semi-Annual Preview";
    if(s==L"MonthlyEnterprise") return L"Monthly Enterprise";
    if(s==L"CurrentPreview") return L"Current Preview";
    if(s==L"BetaChannel") return L"Beta";
    if(s==L"Perpetual") return L"Perpetual";
    return L"Current";
}
static const wchar_t* officeAppText(const std::wstring& s){
    if(s==L"Word") return L"Word"; if(s==L"Excel") return L"Excel"; if(s==L"Access") return L"Access";
    if(s==L"PowerPoint") return L"PowerPoint"; if(s==L"Outlook") return L"Outlook"; if(s==L"OneNote") return L"OneNote";
    if(s==L"Publisher") return L"Publisher"; return L"Teams";
}
static bool officeHas(const std::vector<std::wstring>& v,const std::wstring& x){
    return std::find(v.begin(),v.end(),x)!=v.end();
}
static void officeToggle(std::vector<std::wstring>& v,const std::wstring& x){
    auto it=std::find(v.begin(),v.end(),x);
    if(it==v.end()) v.push_back(x); else v.erase(it);
}
static void officeDropdown(Graphics&g,int x,int y,int w,const std::wstring& labelText,const std::wstring& value,int field){
    text(g,labelText,x,y-18,w,17,9,MUTED());
    fillRound(g,RectF((float)x,(float)y,(float)w,32),INPUTC(),6); strokeRound(g,RectF((float)x,(float)y,(float)w,32),BORDER(),6);
    text(g,value,x+10,y,w-30,32,10,TEXT2()); text(g,L"▾",x+w-22,y,w-10,32,10,SOFT(),false,StringAlignmentCenter);
}
static void officeCheck(Graphics&g,int x,int y,const std::wstring& labelText,bool on,int w){
    fillRound(g,RectF((float)x,(float)y,(float)w,28),on?SELECTED_BG():INPUTC(),6);
    strokeRound(g,RectF((float)x,(float)y,(float)w,28),on?SELECTED_BORDER():BORDER(),6);
    check(g,(float)x+8,(float)y+7,on);
    text(g,labelText,x+29,y,w-35,28,9,TEXT2());
}
static void drawLightning(Graphics&g,float x,float y,float w,float h){
    GraphicsPath p;
    PointF pts[7]={
        PointF(x+w*0.56f,y),
        PointF(x+w*0.12f,y+h*0.53f),
        PointF(x+w*0.43f,y+h*0.53f),
        PointF(x+w*0.30f,y+h),
        PointF(x+w*0.88f,y+h*0.38f),
        PointF(x+w*0.53f,y+h*0.38f),
        PointF(x+w*0.68f,y)
    };
    p.AddPolygon(pts,7);
    SolidBrush b(Color(255,245,190,32));
    g.FillPath(&b,&p);
}

static void office(Graphics&g){
    RECT rc; GetClientRect(gWnd,&rc);
    header(g,L"Office",gLangVi?L"Cài đặt, tải xuống và gỡ Microsoft Office bằng ODT.":L"Install, download and remove Microsoft Office with ODT.");
    // Quick-select lightning: crisp icon, same visual height as the Office title.
    drawLightning(g,313,82,22,27);
    const int left=235, right=rc.right-12, areaW=(int)std::max<LONG>(1L, right-(LONG)left);
    const int panelH=330;
    fillRound(g,RectF((float)left,145,(float)areaW,(float)panelH),PANEL(),9);
    strokeRound(g,RectF((float)left,145,(float)areaW,(float)panelH),BORDER(),9);

    const int gap=8, colW=(areaW-4*gap)/4;
    officeDropdown(g,left,176,colW,L"Phiên bản",gOfficeVersion,0);
    officeDropdown(g,left+colW+gap,176,colW,L"Kiến trúc",gOfficeArch,1);
    officeDropdown(g,left+2*(colW+gap),176,colW,L"Edition",gOfficeEdition,2);
    officeDropdown(g,left+3*(colW+gap),176,colW,L"Kênh cập nhật",officeChannelText(gOfficeChannel),3);

    text(g,L"Ngôn ngữ",left,224,150,18,9,MUTED());
    const std::wstring langs[] = {L"en-us",L"vi-vn",L"ja-jp",L"zh-cn",L"ko-kr"};
    const std::wstring langNames[] = {L"English",L"Tiếng Việt",L"日本語",L"中文",L"한국어"};
    int lx=left, ly=245;
    for(int i=0;i<5;i++){int ww=(i<2?112:82); if(lx+ww>right){lx=left;ly+=32;} officeCheck(g,lx,ly,langNames[i],officeHas(gOfficeLangs,langs[i]),ww);lx+=ww+6;}

    text(g,L"Ứng dụng",left,282,150,18,9,MUTED());
    const std::wstring apps[] = {L"Word",L"Excel",L"Access",L"PowerPoint",L"Outlook",L"OneNote",L"Publisher",L"Teams"};
    int ax=left, ay=303;
    for(int i=0;i<8;i++){int ww=(i==3?102:76); if(ax+ww>right){ax=left;ay+=32;} officeCheck(g,ax,ay,officeAppText(apps[i]),officeHas(gOfficeApps,apps[i]),ww);ax+=ww+6;}

    // Actions: normal buttons keep a visible neutral button background.
    int by=395;
    const int actionGap=10;
    const int bw=(areaW-4*actionGap)/5;
    const int bx[5]={left,left+bw+actionGap,left+2*(bw+actionGap),left+3*(bw+actionGap),left+4*(bw+actionGap)};
    const wchar_t* actionText[5]={L"Cài đặt",L"XML",L"Tải xuống",L"Gỡ Office",L"Thiết đặt"};
    for(int i=0;i<5;i++){
        fillRound(g,RectF((float)bx[i],(float)by,(float)bw,32),BUTTON_BG(),6);
        strokeRound(g,RectF((float)bx[i],(float)by,(float)bw,32),BUTTON_BORDER(),6);
        Color tc=(i==3)?Color(255,205,45,58):BUTTON_TEXT();
        text(g,actionText[i],bx[i],by,bw,32,10,tc,i==0||i==3,StringAlignmentCenter);
    }

    if(gOfficeOpen>=0){
        std::vector<std::wstring> opts;
        if(gOfficeOpen==0) opts=officeVersions();
        else if(gOfficeOpen==1) opts=officeArchs();
        else if(gOfficeOpen==2) opts=officeEditions();
        else opts=officeChannels();
        const int dx=left+gOfficeOpen*(colW+gap), dy=208, oh=28;
        fillRound(g,RectF((float)dx,(float)dy,(float)colW,(float)(opts.size()*oh+4)),INPUTC(),6);
        strokeRound(g,RectF((float)dx,(float)dy,(float)colW,(float)(opts.size()*oh+4)),BORDER(),6);
        for(size_t i=0;i<opts.size();++i) text(g,gOfficeOpen==3?officeChannelText(opts[i]):opts[i],dx+8,dy+2+(int)i*oh,colW-16,oh,9,TEXT2());
    }
}

static void officeSettingCheck(Graphics&g,int x,int y,const std::wstring& labelText,bool on,int w=190){
    check(g,(float)x,(float)y,on);
    text(g,labelText,x+22,y-5,w-22,24,9,TEXT2());
}
static void officeSettingBox(Graphics&g,int x,int y,int w,const std::wstring& value){
    fillRound(g,RectF((float)x,(float)y,(float)w,28),INPUTC(),4);
    strokeRound(g,RectF((float)x,(float)y,(float)w,28),BORDER(),4);
    text(g,value,x+8,y,w-16,28,9,TEXT2());
}
static void officeSettings(Graphics&g){
    RECT rc; GetClientRect(gWnd,&rc);
    fillRound(g,RectF(210,52,(float)std::max<LONG>(1L,rc.right-210),(float)std::max<LONG>(1L,rc.bottom-52)),BG(),0);
    const int px=220, py=60;
    const int pw=(int)std::max<LONG>(560L,std::min<LONG>(600L,rc.right-230L));
    const int ph=(int)std::max<LONG>(440L,std::min<LONG>(450L,rc.bottom-68L));
    fillRound(g,RectF((float)px,(float)py,(float)pw,(float)ph),PANEL(),7);
    strokeRound(g,RectF((float)px,(float)py,(float)pw,(float)ph),BORDER(),7);

    const wchar_t* tabs[]={L"Font",L"Số trang",L"Hướng giấy",L"Mục lục",L"Bảo mật",L"Add-in"};
    const int tabGap=4, tabW=(pw-24-5*tabGap)/6;
    int tx=px+12;
    for(int i=0;i<6;i++){
        bool a=(i==gOfficeSettingsTab);
        fillRound(g,RectF((float)tx,(float)py+6,(float)tabW,30),a?SELECTED_BG():BUTTON_BG(),4);
        strokeRound(g,RectF((float)tx,(float)py+6,(float)tabW,30),a?SELECTED_BORDER():BUTTON_BORDER(),4);
        text(g,tabs[i],tx,py+6,tabW,30,9,a?TEXTC():TEXT2(),a,StringAlignmentCenter);
        tx+=tabW+tabGap;
    }

    officeSettingCheck(g,px+14,py+42,L"Luôn tạo tài liệu mới",gOfficeAlwaysNew,190);
    officeSettingCheck(g,px+295,py+42,L"Hiển thị Word sau khi áp dụng",gOfficeShowWord,220);

    const int gx=px+10, gy=py+73, gw=pw-20;
    fillRound(g,RectF((float)gx,(float)gy,(float)gw,122),gDark?Color(255,18,22,28):Color(255,252,253,254),3);
    strokeRound(g,RectF((float)gx,(float)gy,(float)gw,122),BORDER(),3);
    text(g,L"Thiết lập Font - Đoạn văn",gx+10,gy+3,260,20,10,GREEN(),false);
    text(g,L"Font:",gx+12,gy+25,50,20,9,TEXT2()); officeSettingBox(g,gx+62,gy+22,170,L"Times New Roman");
    text(g,L"Cỡ:",gx+242,gy+25,32,20,9,TEXT2()); officeSettingBox(g,gx+278,gy+22,52,L"14");
    officeSettingCheck(g,gx+345,gy+24,L"Mở Ruler",gOfficeRuler,110);
    text(g,L"Giãn dòng:",gx+12,gy+58,70,20,9,TEXT2()); officeSettingBox(g,gx+82,gy+55,64,L"1.15");
    text(g,L"Cách đoạn:",gx+158,gy+58,66,20,9,TEXT2()); text(g,L"Trước:",gx+226,gy+58,46,20,9,TEXT2()); officeSettingBox(g,gx+272,gy+55,46,L"3"); text(g,L"Sau:",gx+326,gy+58,34,20,9,TEXT2()); officeSettingBox(g,gx+360,gy+55,46,L"3");
    text(g,L"Thụt đầu dòng:",gx+12,gy+91,88,20,9,TEXT2()); officeSettingBox(g,gx+102,gy+88,130,L"(Không)"); text(g,L"Giá trị:",gx+242,gy+91,48,20,9,TEXT2()); officeSettingBox(g,gx+290,gy+88,58,L"1.27"); text(g,L"cm",gx+354,gy+91,25,20,9,TEXT2());
    text(g,L"Khổ giấy:",gx+405,gy+91,55,20,9,TEXT2()); officeSettingBox(g,gx+462,gy+88,80,L"A4"); text(g,L"(21 x 29.7 cm)",gx+548,gy+91,100,20,9,MUTED());

    const int sy=gy+130;
    fillRound(g,RectF((float)gx,(float)sy,(float)gw,63),gDark?Color(255,18,22,28):Color(255,252,253,254),3);
    strokeRound(g,RectF((float)gx,(float)sy,(float)gw,63),BORDER(),3);
    text(g,L"Tóm tắt tác vụ tự động",gx+10,sy+3,250,20,10,GREEN());
    text(g,L"✓  Word: Khổ giấy/font/cỡ/giãn dòng/đoạn/thụt lề theo cài đặt",gx+14,sy+23,gw-28,16,8,TEXT2());
    text(g,L"✓  Word: Tắt kiểm tra chính tả / grammar    ✓  Excel: Font mặc định theo cài đặt",gx+14,sy+39,gw-28,16,8,TEXT2());

    const int ay=sy+70;
    fillRound(g,RectF((float)gx,(float)ay,(float)gw,76),gDark?Color(255,18,22,28):Color(255,252,253,254),3);
    strokeRound(g,RectF((float)gx,(float)ay,(float)gw,76),BORDER(),3);
    text(g,L"Tùy chọn nâng cao",gx+10,ay+3,250,20,10,ACCENT());
    officeSettingCheck(g,gx+12,ay+26,L"Đặt làm mặc định Normal.dotm",gOfficeNormalTemplate,205);
    officeSettingCheck(g,gx+245,ay+26,L"Tắt Protected View",gOfficeProtectedView,170);
    officeSettingCheck(g,gx+425,ay+26,L"Bỏ Start Screen",gOfficeStartScreen,150);
    text(g,L"Lề (cm):",gx+12,ay+51,55,18,8,TEXT2());
    text(g,L"Trên:",gx+76,ay+51,35,18,8,TEXT2()); officeSettingBox(g,gx+112,ay+48,40,L"2");
    text(g,L"Dưới:",gx+163,ay+51,35,18,8,TEXT2()); officeSettingBox(g,gx+199,ay+48,40,L"2");
    text(g,L"Trái:",gx+250,ay+51,32,18,8,TEXT2()); officeSettingBox(g,gx+283,ay+48,40,L"3");
    text(g,L"Phải:",gx+334,ay+51,32,18,8,TEXT2()); officeSettingBox(g,gx+367,ay+48,46,L"1.5");

    const int by=py+ph-38;
    const int bw=108, bgap=10;
    fillRound(g,RectF((float)(px+pw-230),(float)by,(float)bw,30),BUTTON_BG(),5);
    strokeRound(g,RectF((float)(px+pw-230),(float)by,(float)bw,30),BUTTON_BORDER(),5);
    text(g,L"✓  Áp dụng",px+pw-230,by,bw,30,10,TEXT2(),true,StringAlignmentCenter);
    fillRound(g,RectF((float)(px+pw-112),(float)by,(float)bw,30),BUTTON_BG(),5);
    strokeRound(g,RectF((float)(px+pw-112),(float)by,(float)bw,30),BUTTON_BORDER(),5);
    text(g,L"×  Thoát",px+pw-112,by,bw,30,10,TEXT2(),false,StringAlignmentCenter);
}

static void oem(Graphics&g){header(g,L"OEM",gLangVi?L"Chỉnh sửa thông tin OEM của Windows.":L"Edit Windows OEM information.");fillRound(g,RectF(235,145,600,285),PANEL(),9);strokeRound(g,RectF(235,145,600,285),BORDER(),9);const wchar_t*l[]={L"Nhà sản xuất",L"Model",L"Hỗ trợ",L"Điện thoại"};const wchar_t*e[]={L"Manufacturer",L"Model",L"Support",L"Phone"};for(int i=0;i<4;i++){text(g,gLangVi?l[i]:e[i],255,158+i*57,145,22,10,SOFT());fillRound(g,RectF(410,154+i*57,390,32),INPUTC(),5);}fillRound(g,RectF(655,382,145,30),BORDER(),6);text(g,gLangVi?L"Áp dụng":L"Apply",655,382,145,30,10,TEXT2(),false,StringAlignmentCenter);}
static void about(Graphics&g){drawImage(g,img(gDark?L"assets/VAssistant-dark.png":L"assets/VAssistant.png"),RectF(520,105,90,90));text(g,L"VAssistant",400,205,350,38,25,TEXTC(),true,StringAlignmentCenter);text(g,L"v1.0.0",400,240,350,22,10,MUTED(),false,StringAlignmentCenter);text(g,gLangVi?L"Bộ công cụ hỗ trợ cài đặt, cấu hình, sửa lỗi và quản lý Windows.":L"A utility suite for Windows installation, configuration, repair and management.",340,275,470,45,11,MUTED(),false,StringAlignmentCenter);fillRound(g,RectF(390,345,370,150),PANEL(),8);strokeRound(g,RectF(390,345,370,150),BORDER(),8);text(g,gLangVi?L"Tác giả":L"Author",410,357,130,25,10,MUTED());text(g,L"bbkim",600,357,130,25,10,TEXT2(),false,StringAlignmentFar);text(g,gLangVi?L"Giao diện":L"Theme",410,392,130,25,10,MUTED());fillRound(g,RectF(545,389,92,28),gDark?ACCENT():BORDER(),6);text(g,L"Dark",545,389,92,28,10,gDark?WHITE():TEXT2(),false,StringAlignmentCenter);fillRound(g,RectF(643,389,92,28),!gDark?ACCENT():BORDER(),6);text(g,L"Light",643,389,92,28,10,!gDark?WHITE():TEXT2(),false,StringAlignmentCenter);text(g,gLangVi?L"Thay đổi giao diện sẽ áp dụng ngay.":L"Theme changes apply immediately.",410,430,325,22,9,MUTED());text(g,gLangVi?L"Cập nhật":L"Update",410,458,130,22,10,MUTED());text(g,gLangVi?L"Kiểm tra cập nhật":L"Check for updates",600,458,130,22,10,TEXT2(),false,StringAlignmentFar);}static void paint(HWND h){RECT rc;GetClientRect(h,&rc);if(rc.right<=0||rc.bottom<=0)return;Bitmap bm(rc.right,rc.bottom,PixelFormat32bppPARGB);Graphics g(&bm);g.SetSmoothingMode(SmoothingModeAntiAlias);g.Clear(BG());titlebar(g,rc.right);sidebar(g,rc.bottom);switch(gPage){case 0:setup(g);break;case 1:portable(g);break;case 2:office(g);break;case 3:printer(g);break;case 4:bloat(g);break;case 5:oem(g);break;default:about(g);}if(gPage==2&&gOfficeSettingsOpen)officeSettings(g);HDC dc=GetDC(h);Graphics screen(dc);screen.DrawImage(&bm,0,0);ReleaseDC(h,dc);}

static void drawDownloadButton(Graphics&g,int x,int y,int w,const std::wstring&normal){fillRound(g,RectF((float)x,(float)y,(float)w,26),BORDER(),6);if(InterlockedCompareExchange(&gDownloading,0,0)){float pct=InterlockedCompareExchange(&gDownloadProgress,0,0)/100.0f;fillRound(g,RectF((float)x,(float)y,(float)(w*pct),26),ACCENT(),6);std::wstring s=(gDownloadTotal>1?std::to_wstring(gDownloadIndex)+L"/"+std::to_wstring(gDownloadTotal)+L"  ":L"")+std::to_wstring((int)(pct*100))+L"%";text(g,s,x,y,w,26,10,WHITE(),false,StringAlignmentCenter);}else{text(g,normal,x,y,w,26,10,TEXT2(),false,StringAlignmentCenter);}}

static void softClick(size_t i){if(i>=gSoftChecked.size())return;gSoftChecked[i]=!gSoftChecked[i];gSelected=(int)std::count(gSoftChecked.begin(),gSoftChecked.end(),true);gSelectAll=!gSoftChecked.empty()&&gSelected==(int)gSoftChecked.size();InvalidateRect(gWnd,nullptr,FALSE);}
static void bloatClick(size_t i){if(i>=gBloatChecked.size())return;gBloatChecked[i]=!gBloatChecked[i];InvalidateRect(gWnd,nullptr,FALSE);}
static int pageViewportTop(){ return gPage==0?178:140; }
static void clampScroll(){
    RECT r; GetClientRect(gWnd,&r);
    int view=std::max(1, static_cast<int>(r.bottom-pageViewportTop()-10));
    int maxScroll=std::max(0,gContentHeight-view);
    if(gScrollY<0) gScrollY=0;
    if(gScrollY>maxScroll) gScrollY=maxScroll;
}
static bool hitPortableCard(int x,int y,size_t*idx){
    RECT rc; GetClientRect(gWnd,&rc); int left=235,right=rc.right-12,gap=8,minCard=155;
    int cols=gridColumns(right-left,minCard,gap,4); int cardW=gridCardWidth(right-left,cols,gap);
    int top=145;
    int sy=y+gScrollY-top; if(sy<0) return false;
    int row=sy/(46+gap), col=(x-left)/(cardW+gap);
    if(row<0||col<0||col>=cols) return false;
    int localX=x-(left+col*(cardW+gap)), localY=sy-row*(46+gap);
    if(localX<0||localX>=cardW||localY<0||localY>=46) return false;
    size_t i=(size_t)row*cols+col; if(i>=gPortable.size()) return false; *idx=i; return true;
}
static bool hitSoftwareCard(int x,int y,size_t*idx){
    RECT rc; GetClientRect(gWnd,&rc); int left=235,right=rc.right-12,gap=8,minCard=155;
    int cols=gridColumns(right-left,minCard,gap,4); int cardW=gridCardWidth(right-left,cols,gap);
    int top=182; int sy=y+gScrollY-top; if(sy<0) return false;
    int row=sy/(46+gap), col=(x-left)/(cardW+gap);
    if(row<0||col<0||col>=cols) return false;
    int localX=x-(left+col*(cardW+gap)), localY=sy-row*(46+gap);
    if(localX<0||localX>cardW||localY<0||localY>46) return false;
    size_t i=(size_t)row*cols+col; if(i>=gSoftware.size()) return false; *idx=i; return true;
}

static OfficeConfig currentOfficeConfig(){
    OfficeConfig c;
    c.version=gOfficeVersion; c.arch=(gOfficeArch==L"x64"?L"64":L"32");
    c.edition=gOfficeEdition; c.channel=(gOfficeVersion==L"365"?gOfficeChannel:L"Perpetual");
    c.languages=gOfficeLangs; c.apps=gOfficeApps; return c;
}
static void officeQuickSelect(){
    gOfficeVersion=L"2024"; gOfficeEdition=L"ProPlus"; gOfficeChannel=L"Perpetual";
    gOfficeLangs={L"vi-vn"};
    gOfficeApps={L"Word",L"Excel",L"Access",L"PowerPoint",L"Outlook"};
    gOfficeOpen=-1; GuiSetStatus(L"Đã chọn nhanh: Office 2024 ProPlus",false); InvalidateRect(gWnd,nullptr,FALSE);
}
static bool officeSelectPopup(int x,int y){
    if(gOfficeOpen<0) return false;
    RECT r; GetClientRect(gWnd,&r);
    const int left=235,right=r.right-12,gap=8,areaW=(int)std::max<LONG>(1L,right-(LONG)left);
    const int colW=(areaW-4*gap)/4, dx=left+gOfficeOpen*(colW+gap), dy=208, oh=28;
    std::vector<std::wstring> opts=(gOfficeOpen==0?officeVersions():gOfficeOpen==1?officeArchs():gOfficeOpen==2?officeEditions():officeChannels());
    if(x>=dx&&x<dx+colW&&y>=dy&&y<dy+(int)opts.size()*oh+4){
        int n=(y-dy-2)/oh;
        if(n>=0&&n<(int)opts.size()){
            if(gOfficeOpen==0){
                gOfficeVersion=opts[n];
                auto ed=officeEditions(); if(std::find(ed.begin(),ed.end(),gOfficeEdition)==ed.end()) gOfficeEdition=L"ProPlus";
                gOfficeChannel=officeChannels().front();
            } else if(gOfficeOpen==1) gOfficeArch=opts[n];
            else if(gOfficeOpen==2) gOfficeEdition=opts[n];
            else gOfficeChannel=opts[n];
            gOfficeOpen=-1; InvalidateRect(gWnd,nullptr,FALSE); return true;
        }
    }
    gOfficeOpen=-1; InvalidateRect(gWnd,nullptr,FALSE); return true;
}
static bool officeSettingsClick(int x,int y){
    RECT r; GetClientRect(gWnd,&r);
    const int px=220, py=60;
    const int pw=(int)std::max<LONG>(560L,std::min<LONG>(600L,r.right-230L));
    const int ph=(int)std::max<LONG>(440L,std::min<LONG>(450L,r.bottom-68L));
    if(x<px||x>px+pw||y<py||y>py+ph){gOfficeSettingsOpen=false;InvalidateRect(gWnd,nullptr,FALSE);return true;}
    const int tabGap=4, tabW=(pw-24-5*tabGap)/6;
    if(y>=py+6&&y<py+36){
        int idx=(x-(px+12))/(tabW+tabGap);
        if(x>=px+12&&idx>=0&&idx<6) gOfficeSettingsTab=idx;
        InvalidateRect(gWnd,nullptr,FALSE); return true;
    }
    if(y>=py+38&&y<py+70){
        if(x>=px+10&&x<px+260) gOfficeAlwaysNew=!gOfficeAlwaysNew;
        else if(x>=px+280&&x<px+545) gOfficeShowWord=!gOfficeShowWord;
        InvalidateRect(gWnd,nullptr,FALSE); return true;
    }
    const int gx=px+10, gy=py+73, gw=pw-20;
    if(y>=gy+15&&y<gy+55&&x>=gx+330&&x<gx+gw-10){gOfficeRuler=!gOfficeRuler;InvalidateRect(gWnd,nullptr,FALSE);return true;}
    const int sy=gy+130, ay=sy+70;
    if(y>=ay+18&&y<ay+48){
        if(x>=gx&&x<gx+230) gOfficeNormalTemplate=!gOfficeNormalTemplate;
        else if(x>=gx+230&&x<gx+415) gOfficeProtectedView=!gOfficeProtectedView;
        else if(x>=gx+415&&x<gx+gw) gOfficeStartScreen=!gOfficeStartScreen;
        InvalidateRect(gWnd,nullptr,FALSE); return true;
    }
    const int by=py+ph-38;
    if(y>=by&&y<=by+30){
        if(x>=px+pw-230&&x<px+pw-112){GuiSetStatus(L"Đã áp dụng thiết đặt Office",false);gOfficeSettingsOpen=false;InvalidateRect(gWnd,nullptr,FALSE);return true;}
        if(x>=px+pw-112&&x<px+pw){gOfficeSettingsOpen=false;InvalidateRect(gWnd,nullptr,FALSE);return true;}
    }
    return true;
}

static bool officeClick(int x,int y){
    if(gOfficeSettingsOpen) return officeSettingsClick(x,y);
    RECT r; GetClientRect(gWnd,&r);
    const int left=235,right=r.right-12,areaW=(int)std::max<LONG>(1L,right-(LONG)left),gap=8;
    const int colW=(areaW-4*gap)/4;
    if(officeSelectPopup(x,y)) return true;
    if(y>=176&&y<208){
        for(int i=0;i<4;i++){int dx=left+i*(colW+gap);if(x>=dx&&x<dx+colW){gOfficeOpen=i;InvalidateRect(gWnd,nullptr,FALSE);return true;}}
    }
    const std::wstring langs[] = {L"en-us",L"vi-vn",L"ja-jp",L"zh-cn",L"ko-kr"};
    const int lw[] = {112,112,82,82,82};
    int lx=left,ly=245;
    for(int i=0;i<5;i++){
        int ww=lw[i]; if(lx+ww>right){lx=left;ly+=32;}
        if(x>=lx&&x<lx+ww&&y>=ly&&y<ly+28){officeToggle(gOfficeLangs,langs[i]);if(gOfficeLangs.empty()) GuiSetStatus(L"Lỗi: Phải chọn ít nhất 1 ngôn ngữ",true);else GuiSetStatus(L"Sẵn sàng",false);InvalidateRect(gWnd,nullptr,FALSE);return true;}
        lx+=ww+6;
    }
    const std::wstring apps[] = {L"Word",L"Excel",L"Access",L"PowerPoint",L"Outlook",L"OneNote",L"Publisher",L"Teams"};
    int ax=left,ay=303;
    for(int i=0;i<8;i++){
        int ww=(i==3?102:76); if(ax+ww>right){ax=left;ay+=32;}
        if(x>=ax&&x<ax+ww&&y>=ay&&y<ay+28){officeToggle(gOfficeApps,apps[i]);if(gOfficeApps.empty()) GuiSetStatus(L"Lỗi: Phải chọn ít nhất 1 ứng dụng",true);else GuiSetStatus(L"Sẵn sàng",false);InvalidateRect(gWnd,nullptr,FALSE);return true;}
        ax+=ww+6;
    }
    // Lightning quick-select is now next to the Office title.
    if(x>=307&&x<342&&y>=78&&y<113){officeQuickSelect();return true;}
    const int by=395, actionGap=10, bw=(areaW-4*actionGap)/5;
    const int bx[5]={left,left+bw+actionGap,left+2*(bw+actionGap),left+3*(bw+actionGap),left+4*(bw+actionGap)};
    for(int i=0;i<5;i++){
        int xx=bx[i];
        if(x>=xx&&x<xx+bw&&y>=by&&y<by+32){
            if(i==4){gOfficeSettingsOpen=true;gOfficeOpen=-1;InvalidateRect(gWnd,nullptr,FALSE);return true;}
            if(i==3){OfficeRunAction(currentOfficeConfig(),4);return true;}
            if(gOfficeLangs.empty()||gOfficeApps.empty()){GuiSetStatus(L"Lỗi: Chọn ngôn ngữ và ứng dụng",true);return true;}
            OfficeRunAction(currentOfficeConfig(),i==0?1:i==1?3:2); return true;
        }
    }
    return false;
}

static void click(int x,int y){
    RECT r;GetClientRect(gWnd,&r);int w=r.right;
    if(y<52){
        if(x>w-160&&x<w-112){gLangVi=!gLangVi;InvalidateRect(gWnd,nullptr,FALSE);return;}
        if(x>w-112&&x<w-75){ShowWindow(gWnd,SW_MINIMIZE);return;}
        if(x>w-75&&x<w-40){gMax=!gMax;ShowWindow(gWnd,gMax?SW_MAXIMIZE:SW_RESTORE);return;}
        if(x>w-40){PostMessageW(gWnd,WM_CLOSE,0,0);return;}
        gDragging=true;GetCursorPos(&gDrag);SetCapture(gWnd);return;
    }
    if(x<210&&y>=92&&y<92+7*45){gPage=(y-92)/45;gScrollY=0;gContentHeight=0;InvalidateRect(gWnd,nullptr,FALSE);return;}
    if(gPage==0){
        if(y>=134&&y<172){
            const int left=235, right=r.right-12, refreshW=68, installW=120, gap=8;
            const int refreshX=right-refreshW, installX=refreshX-gap-installW;
            if(x>=left&&x<left+150){gSelectAll=!gSelectAll;for(size_t i=0;i<gSoftChecked.size();++i)gSoftChecked[i]=gSelectAll;gSelected=gSelectAll?(int)gSoftChecked.size():0;InvalidateRect(gWnd,nullptr,FALSE);return;}
            if(x>=installX&&x<installX+installW){StartSelectedDownloads();return;}
            if(x>=refreshX&&x<refreshX+refreshW){LoadData();gSelected=0;gSelectAll=false;gScrollY=0;GuiSetStatus(gLangVi?L"Đã làm mới":L"Refreshed",false);return;}
        }
        size_t i; if(hitSoftwareCard(x,y,&i)){softClick(i);return;}
    }
    if(gPage==2){ if(officeClick(x,y)) return; }
    if(gPage==1){size_t i;if(hitPortableCard(x,y,&i)){StartDownload(gPortable[i]);return;}}
    if(gPage==4){
        // Bloatware click hit-test follows the same flow as drawing, adjusted for scroll.
        const int left=235,right=r.right-12,gap=8,minCard=210;int cols=gridColumns(right-left,minCard,gap,4),cardW=gridCardWidth(right-left,cols,gap);
        float yy=145-(float)gScrollY;std::string last;
        for(size_t i=0;i<gBloat.size();){
            if(gBloat[i].group!=last){last=gBloat[i].group;yy+=29;}
            size_t j=i;while(j<gBloat.size()&&gBloat[j].group==last)++j;
            for(size_t k=i;k<j;++k){int n=(int)(k-i);float xx=(float)(left+(n%cols)*(cardW+gap)),cy=yy+(float)((n/cols)*46);if(x>=xx&&x<xx+cardW&&y>=cy&&y<cy+39){bloatClick(k);return;}}
            yy+=((int)(j-i)+cols-1)/cols*46+17;i=j;
        }
    }
    if(gPage==6&&y>=385&&y<=425){if(x>=545&&x<637){gDark=true;InvalidateRect(gWnd,nullptr,FALSE);return;}if(x>=643&&x<735){gDark=false;InvalidateRect(gWnd,nullptr,FALSE);return;}}
}
LRESULT CALLBACK GuiWindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        gWnd = h;
        LoadData();
        gSelected = 0;
        gSelectAll = false;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        paint(h);
        return 0;
    }
    case WM_LBUTTONDOWN:
        click(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        return 0;
    case WM_NCLBUTTONDOWN:
        if (w == HTLEFT || w == HTRIGHT || w == HTTOP || w == HTBOTTOM ||
            w == HTTOPLEFT || w == HTTOPRIGHT || w == HTBOTTOMLEFT || w == HTBOTTOMRIGHT) {
            gResizing = true;
            gResizeHit = (UINT)w;
            GetWindowRect(h, &gResizeStart);
            GetCursorPos(&gResizePoint);
            SetCapture(h);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (gResizing) {
            POINT p; GetCursorPos(&p);
            RECT nr = gResizeStart;
            const int dx = p.x - gResizePoint.x;
            const int dy = p.y - gResizePoint.y;
            if (gResizeHit == HTLEFT || gResizeHit == HTTOPLEFT || gResizeHit == HTBOTTOMLEFT) nr.left += dx;
            if (gResizeHit == HTRIGHT || gResizeHit == HTTOPRIGHT || gResizeHit == HTBOTTOMRIGHT) nr.right += dx;
            if (gResizeHit == HTTOP || gResizeHit == HTTOPLEFT || gResizeHit == HTTOPRIGHT) nr.top += dy;
            if (gResizeHit == HTBOTTOM || gResizeHit == HTBOTTOMLEFT || gResizeHit == HTBOTTOMRIGHT) nr.bottom += dy;
            const int minW = 720, minH = 460;
            if (nr.right - nr.left < minW) {
                if (gResizeHit == HTLEFT || gResizeHit == HTTOPLEFT || gResizeHit == HTBOTTOMLEFT) nr.left = nr.right - minW;
                else nr.right = nr.left + minW;
            }
            if (nr.bottom - nr.top < minH) {
                if (gResizeHit == HTTOP || gResizeHit == HTTOPLEFT || gResizeHit == HTTOPRIGHT) nr.top = nr.bottom - minH;
                else nr.bottom = nr.top + minH;
            }
            SetWindowPos(h, nullptr, nr.left, nr.top, nr.right - nr.left, nr.bottom - nr.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (gDragging) {
            POINT p;
            GetCursorPos(&p);
            RECT r;
            GetWindowRect(h, &r);
            SetWindowPos(h, nullptr, r.left + p.x - gDrag.x, r.top + p.y - gDrag.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            gDrag = p;
        }
        return 0;
    case WM_LBUTTONUP:
        if (gResizing) {
            gResizing = false;
            gResizeHit = 0;
            ReleaseCapture();
            return 0;
        }
        if (gDragging) {
            gDragging = false;
            ReleaseCapture();
        }
        return 0;
    case WM_NCLBUTTONUP:
        if (gResizing) {
            gResizing = false;
            gResizeHit = 0;
            ReleaseCapture();
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(h, &p);
        RECT r;
        GetClientRect(h, &r);
        int b = 6;
        const bool left = p.x < b, right = p.x > r.right - b;
        const bool top = p.y < b, bottom = p.y > r.bottom - b;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        return HTCLIENT;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(w);
        if(gPage==0 || gPage==1 || gPage==4){
            gScrollY -= delta / 2;
            clampScroll();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mi = reinterpret_cast<MINMAXINFO*>(l);
        mi->ptMinTrackSize.x = 720;
        mi->ptMinTrackSize.y = 460;
        return 0;
    }
    case WM_SIZE:
        clampScroll();
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_DOWNLOAD_DONE:
        HandleDownloadDone(h);
        return 0;
    case WM_OFFICE_DONE:
        OfficeHandleDoneForGui(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

bool GuiInitialize(HINSTANCE h, int nCmdShow) {
    // Set a stable AppUserModelID when the Windows shell API is available.
    // Resolve it dynamically so older MinGW headers do not break the build.
    HMODULE shell = GetModuleHandleW(L"shell32.dll");
    if (!shell) shell = LoadLibraryW(L"shell32.dll");
    if (shell) {
        using SetAppIdFn = HRESULT (WINAPI*)(PCWSTR);
        auto setAppId = reinterpret_cast<SetAppIdFn>(GetProcAddress(shell, "SetCurrentProcessExplicitAppUserModelID"));
        if (setAppId) setAppId(APP_ID);
    }
    GdiplusStartupInput gi;
    if (GdiplusStartup(&gGdi, &gi, nullptr) != Ok) return false;

    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = h;
    wc.lpfnWndProc = GuiWindowProc;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(h, MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) { GdiplusShutdown(gGdi); gGdi = 0; return false; }

    HWND w = CreateWindowExW(WS_EX_APPWINDOW, CLASS_NAME, L"VAssistant",
        WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 840, 520, nullptr, nullptr, h, nullptr);
    if (!w) { GdiplusShutdown(gGdi); gGdi = 0; return false; }

    gWnd = w;
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    gOfficeArch = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
                   si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) ? L"x64" : L"x86";
    HICON icon = LoadIconW(h, MAKEINTRESOURCEW(101));
    SendMessageW(w, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SendMessageW(w, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    ShowWindow(w, nCmdShow ? nCmdShow : SW_SHOWNORMAL);
    UpdateWindow(w);
    return true;
}

void GuiShutdown() {
    gImages.clear();
    if (gGdi) { GdiplusShutdown(gGdi); gGdi = 0; }
}
