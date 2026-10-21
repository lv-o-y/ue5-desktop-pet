// Fill out your copyright notice in the Description page of Project Settings.

#include "MyActorAA2.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "UObject/ConstructorHelpers.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "GameFramework/PlayerController.h"

static WNDPROC GOriginalWndProc = nullptr;
static POINT GDragStart = { 0, 0 };
static bool  GDragging = false;

// 判断屏幕坐标是否落在「标题栏区域」（顶部 40px + 四周 10px）
static bool IsTitleArea(HWND hwnd, int ScreenX, int ScreenY)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    const int lx = ScreenX - rc.left;
    const int ly = ScreenY - rc.top;
    const int w  = rc.right - rc.left;
    const int h  = rc.bottom - rc.top;
    return (ly < 40 || lx < 10 || lx > w - 10 || ly > h - 10);
}

static LRESULT CALLBACK PetWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 自己实现拖动：不进 Windows 的模态循环，游戏不会卡
    if (msg == WM_LBUTTONDOWN)
    {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ClientToScreen(hwnd, &pt);
        if (IsTitleArea(hwnd, pt.x, pt.y))
        {
            GetCursorPos(&GDragStart);
            GDragging = true;
            SetCapture(hwnd);
            return 0;
        }
    }

    if (msg == WM_MOUSEMOVE && GDragging)
    {
        POINT Cur;
        GetCursorPos(&Cur);
        RECT rc;
        GetWindowRect(hwnd, &rc);
        SetWindowPos(hwnd, nullptr,
            rc.left + (Cur.x - GDragStart.x),
            rc.top + (Cur.y - GDragStart.y),
            0, 0, SWP_NOSIZE | SWP_NOZORDER);
        GDragStart = Cur;
        return 0;
    }

    if (msg == WM_LBUTTONUP && GDragging)
    {
        GDragging = false;
        ReleaseCapture();
        return 0;
    }

    return CallWindowProc(GOriginalWndProc, hwnd, msg, wParam, lParam);
}

// Sets default values
AMyActorAA2::AMyActorAA2()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CubeAsset.Object);
	}

}

// Called when the game starts or when spawned
void AMyActorAA2::BeginPlay()
{
    Super::BeginPlay();

    // 站到摄像机正前方 300 单位，保证一开始就在画面里
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        FVector CamLoc;
        FRotator CamRot;
        PC->GetPlayerViewPoint(CamLoc, CamRot);
        SetActorLocation(CamLoc + CamRot.Vector() * 300.0f);
    }

    if (!GEngine || !GEngine->GameViewport) return;

    TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
    if (!Window.IsValid()) return;

    TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
    if (!NativeWindow.IsValid()) return;

    HWND hwnd = static_cast<HWND>(NativeWindow->GetOSWindowHandle());
    if (!hwnd) return;

    SetWindowTextW(hwnd, TEXT("Desktop Pet v0.1"));

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    style |= WS_POPUP;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // --- D4a: 置顶 ---
    const int W = 600, H = 400;
    const int X = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    const int Y = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    SetWindowPos(hwnd, HWND_TOPMOST, X, Y, W, H, SWP_NOACTIVATE);

    // --- D4b: 整体半透明 ---
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);

    GOriginalWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)PetWndProc);

    // 桌宠不抓鼠标：显示光标，不锁定视口
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->bShowMouseCursor = true;

        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);

        // 让 Actor 能收到鼠标点击
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
    }
}

// 被鼠标点击时调用
void AMyActorAA2::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	CurrentState = EPetState::Happy;
	StateTimer = 0.0f;
	SetActorScale3D(FVector(1.2f));   // 变胖一点
}

// Called every frame
void AMyActorAA2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ① 一直转（Happy 时转更快）
	const float SpeedMul = (CurrentState == EPetState::Happy) ? 5.0f : 1.0f;
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw += RotateSpeed * SpeedMul * DeltaTime;
	SetActorRotation(NewRotation);

	// ② 状态计时
	StateTimer += DeltaTime;

	// ③ 状态切换
	if (CurrentState == EPetState::Idle && StateTimer > 2.0f)
	{
		CurrentState = EPetState::Walking;
		StateTimer = 0.0f;
	}
	else if (CurrentState == EPetState::Walking && StateTimer > 3.0f)
	{
		CurrentState = EPetState::Idle;
		StateTimer = 0.0f;
	}
	else if (CurrentState == EPetState::Happy && StateTimer > 2.0f)
	{
		CurrentState = EPetState::Idle;
		StateTimer = 0.0f;
		SetActorScale3D(FVector(1.0f));   // 恢复原来的大小
	}

	// ④ 走动时移动位置
	if (CurrentState == EPetState::Walking)
	{
		FVector Loc = GetActorLocation();
		Loc.Y += MoveSpeed * MoveDirection * DeltaTime;

		if (Loc.Y > 120.0f) MoveDirection = -1.0f;
		if (Loc.Y < -120.0f) MoveDirection = 1.0f;

		SetActorLocation(Loc);
	}
}

