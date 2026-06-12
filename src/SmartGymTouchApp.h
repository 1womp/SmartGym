#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <FS.h>
#include <freertos/semphr.h>
#include "CalibrationService.h"
#include "DeviceConfigStore.h"
#include "FirebaseService.h"
#include "LocalPersistenceStore.h"
#include "MachineRegistry.h"
#include "RepDetector.h"
#include "RfidService.h"
#include "SensorManager.h"
#include "SessionRecorder.h"
#include "TimeService.h"
#include "UserRegistry.h"

class SmartGymTouchApp {
 public:
  void begin();

 private:
  enum class State {
    Idle,
    Calibration,
    Training,
    Summary
  };

  enum class CalibrationFlowState : uint8_t {
    Idle = 0,
    Intro,
    SetBottomRom,
    SetTopRom,
    RecommendStartWeight,
    ConfirmStartWeight,
    CollectSet,
    AnalyzeSet,
    AskNextSet,
    Result,
    Saving,
    Saved,
    Cancelled
  };

  enum class SessionStage {
    Idle,
    Identify,
    Calibrate,
    Train,
    Rest,
    Summary,
    Logout
  };

  enum class AutoMotionPhase {
    Idle,
    Up,
    Down
  };

  enum class UiScreenMode {
    Main,
    Summary,
    Idle,
    Calibration,
    CalibrationGate,
    Debug,
    Profile
  };
  enum class LogLevel : uint8_t {
    Quiet = 0,
    Normal,
    Verbose
  };

  enum class UploadMemoryMode : uint8_t {
    Normal = 0,
    Constrained,
    Critical
  };

  struct ResolvedRecommendation {
    bool hasRecommendation = false;
    float kg = 0.0f;
    String source;
    String reason;
    uint32_t updatedAtEpoch = 0;
  };

  struct UiCache {
    uint32_t lastSlowUiRefreshMs = 0;
    uint32_t lastChartRefreshMs = 0;
    uint32_t lastAutoMotionMs = 0;
    uint32_t lastCloudServiceMs = 0;
    uint16_t lastDropdownIndex = 0xFFFF;

    String lastTopState;
    String lastUser;
    String lastMachine;
    String lastClock;
    String lastSyncCue;
    String lastSessionTimer;
    String lastBigWeight;
    String lastRep;
    String lastSet;
    String lastRom;
    String lastVelocity;
    String lastSuggestion;
    String lastRepPhase;
    String lastTargetPace;
    String lastPaceTile;
    String lastStatus;
    String lastQuality;
    String lastHistory;
    String lastSessionMeta;
    String lastDebugStatus;
    String lastDebugStatusHardware;
    String lastSummaryUser;
    String lastSummaryResult;
    String lastSummaryDetail;
    String lastSummaryCountdown;
    bool lastIdleVisible = false;
    bool lastSummaryVisible = false;
    bool lastRestVisible = false;
    bool lastCalibrationGateVisible = false;
    bool lastChartVisible = false;
    bool lastChartExpanded = false;
  };

  UserRegistry userRegistry_;
  MachineRegistry machineRegistry_;
  RepDetector repDetector_;
  CalibrationService calibrationService_;
  SessionRecorder sessionRecorder_;
  SessionRecorder uploadScratchRecorder_;
  DeviceConfigStore deviceConfigStore_;
  LocalPersistenceStore localPersistenceStore_;
  TimeService timeService_;
  FirebaseService firebaseService_;
  SensorManager sensorManager_;
  RfidService rfidService_;

  const MachineProfile* machineProfile_ = nullptr;
  UserProfile* activeUser_ = nullptr;
  UserMachineCalibration* activeCalibration_ = nullptr;
  State state_ = State::Idle;
  bool anonymousMode_ = false;
  bool user2Provisioned_ = false;

  float selectedWeightKg_ = 20.0f;
  ResolvedRecommendation resolvedRecommendation_;
  float liveRomPercent_ = 0.0f;
  float simulatedRomPercent_ = 0.0f;
  float lastSimulatedRomPercent_ = 0.0f;
  float simulatedVelocityPctPerSec_ = 0.0f;
  float filteredVelocityPctPerSec_ = 0.0f;
  bool velocityStillLatched_ = true;
  SensorReading lastSensorReading_;

  uint32_t lastTickMs_ = 0;
  uint16_t sessionRepCount_ = 0;
  uint8_t currentSetRepCount_ = 0;
  uint8_t completedSets_ = 0;
  uint32_t setStartedAtMs_ = 0;
  uint32_t lastAcceptedRepAtMs_ = 0;
  uint32_t setPauseCandidateMs_ = 0;
  uint32_t lastMachineCloudPollMs_ = 0;
  uint32_t lastDeviceHeartbeatMs_ = 0;
  uint32_t lastHeartbeatSkipLogMs_ = 0;
  uint32_t lastUploadRetryMs_ = 0;
  uint32_t uploadRetryHoldoffUntilMs_ = 0;
  uint8_t uploadSuccessStreak_ = 0;
  uint32_t lastWebUploadPhaseCompleteMs_ = 0;
  uint32_t lastCloudProfileReadMs_ = 0;
  uint32_t lastCloudHistoryReadMs_ = 0;
  uint32_t lastCloudHistoryTimeoutMs_ = 0;
  uint32_t lastQueuedProfileWriteMs_ = 0;
  String lastCloudProfileReadUid_;
  String lastCloudHistoryReadUid_;
  String lastCloudHistoryTimeoutUid_;
  String lastQueuedProfileWritePath_;
  String lastQueuedProfileWritePayload_;
  bool activeUserProfileDirty_ = false;
  String activeUserProfileDirtyReason_;
  uint32_t lastCloudProfileFetchedEpoch_ = 0;
  String lastGoalNormalizationLogKey_;
  String lastRomMapLogKey_;
  uint32_t sessionStartMs_ = 0;
  uint32_t lastSetCompletedMs_ = 0;
  uint32_t sessionSummaryStartedMs_ = 0;
  uint32_t sessionSummaryMinVisibleUntilMs_ = 0;
  String lastSessionUserName_;
  String lastSessionMachineName_;
  String lastSessionUserUid_;
  uint8_t activeSessionTargetSets_ = 0;
  uint8_t activeSessionTargetRepsMin_ = 0;
  uint8_t activeSessionTargetRepsMax_ = 0;
  uint16_t activeSessionRestSeconds_ = 0;
  float activeSessionWeightKg_ = 0.0f;
  RepMetrics lastCompletedRep_;
  bool hasLastCompletedRep_ = false;
  float sessionBestRepPeakVelocityPctPerSec_ = 0.0f;
  uint8_t poorRepStreak_ = 0;
  bool pendingMachineConfigUpload_ = false;
  bool forceCloudSyncAfterFinish_ = false;
  bool encoderZeroCaptured_ = false;
  bool encoderFullCaptured_ = false;
  uint32_t encoderZeroRaw_ = 0;
  uint32_t encoderFullRaw_ = 0;
  float encoderReferenceDistanceMm_ = 1000.0f;
  float userRomBottomCapturePct_ = 0.0f;
  float userRomTopCapturePct_ = 0.0f;
  bool userRomBottomCaptured_ = false;
  bool userRomTopCaptured_ = false;
  bool encoderCalibrationReady_ = false;
  bool deviceEncoderCalibrationValid_ = false;
  uint32_t deviceEncoderZeroRaw_ = 0;
  uint32_t deviceEncoderFullRaw_ = 0;
  bool deviceEncoderInvertDirection_ = false;
  uint32_t lastUserActivityMs_ = 0;
  uint32_t lastWeightAdjustMs_ = 0;
  uint32_t idleWakeCandidateMs_ = 0;
  uint32_t idleShowCandidateMs_ = 0;
  uint32_t idleHideCandidateMs_ = 0;
  UiScreenMode currentUiScreen_ = UiScreenMode::Main;

  String statusMessage_;
  String lastSessionSummary_;
  String lastScannedUid_;
  String lastRepSummary_;
  String encoderCalibrationSummary_;

  AutoMotionPhase autoMotionPhase_ = AutoMotionPhase::Idle;
  bool autoMotionEnabled_ = false;
  bool cloudEnabled_ = false;
  bool hardwareSensorEnabled_ = false;
  bool hardwareRfidEnabled_ = false;
  bool rfidInitialized_ = false;
  bool firstSensorBootMemLogged_ = false;
  bool sensorSimulationEnabled_ = true;
  bool pendingWorkoutAfterCalibration_ = false;
  bool profileCreateMode_ = false;
  uint8_t profileEditAge_ = 25;
  float profileEditWeightKg_ = 70.0f;
  float profileEditHeightCm_ = 170.0f;
  TrainingGoal profileEditGoal_ = TrainingGoal::Hypertrophy;
  UserGender profileEditGender_ = UserGender::Unspecified;
  String profileEditName_;
  uint32_t profileAdjustDebounceMs_ = 0;
  uint32_t lastButtonEventMs_ = 0;
  uint32_t lastLogicalButtonActionMs_ = 0;
  String lastLogicalButtonAction_;
  uint32_t calibrationActionBusyUntilMs_ = 0;
  uint32_t calibrationStartBlockedUntilMs_ = 0;
  String lastCalibrationLiveLoadLogKey_;
  String lastCalibrationLoadUiLogKey_;
  uint32_t lastScreenSwitchMs_ = 0;
  uint32_t trainingStartBlockedUntilMs_ = 0;
  uint32_t cloudBlockedUntilMs_ = 0;
  CalibrationFlowState calibrationFlowState_ = CalibrationFlowState::Idle;
  uint8_t calibrationCurrentSetIndex_ = 0;
  uint8_t calibrationTargetRepsPerSet_ = 3;
  uint8_t calibrationValidRepsInSet_ = 0;
  uint8_t calibrationRejectedRepsInSet_ = 0;
  float calibrationRepVelocitySum_ = 0.0f;
  float calibrationRepRomSum_ = 0.0f;
  float calibrationRepMinRomSum_ = 0.0f;
  float calibrationRepMaxRomSum_ = 0.0f;
  float calibrationRepDurationSumMs_ = 0.0f;
  float calibrationRepVelocities_[6] = {};
  uint8_t calibrationRepVelocityCount_ = 0;
  float calibrationSuggestedStartWeightKg_ = 0.0f;
  float calibrationCurrentSetWeightKg_ = 0.0f;
  float calibrationNextWeightKg_ = 0.0f;
  bool calibrationHasUserWeightOverride_ = false;
  String calibrationStartWeightSource_;
  String calibrationResultReason_;
  String calibrationResultAction_;
  String calibrationResultConfidence_;
  float calibrationResultRecommendedKg_ = 0.0f;
  MotionTargetConfig calibrationMotionTargets_;
  CalibrationSetSnapshot calibrationSetSnapshots_[UserMachineCalibration::kMaxCalibrationSets];
  uint8_t calibrationSetSnapshotCount_ = 0;

  UiCache uiCache_;

  lv_style_t styleScreen_{};
  lv_style_t stylePanel_{};
  lv_style_t stylePanelDark_{};
  lv_style_t styleHeader_{};
  lv_style_t styleAccentCard_{};
  lv_style_t styleButtonPrimary_{};
  lv_style_t styleButtonSecondary_{};
  lv_style_t styleButtonDanger_{};
  lv_style_t styleButtonGhost_{};
  lv_style_t styleModal_{};
  lv_style_t stylePill_{};

  lv_obj_t* screen_ = nullptr;
  lv_obj_t* summaryScreen_ = nullptr;
  lv_obj_t* summaryPanel_ = nullptr;
  lv_obj_t* summaryBadgeLabel_ = nullptr;
  lv_obj_t* summaryTitleLabel_ = nullptr;
  lv_obj_t* summaryUserLabel_ = nullptr;
  lv_obj_t* summaryResultLabel_ = nullptr;
  lv_obj_t* summaryRepsLabel_ = nullptr;
  lv_obj_t* summaryRomLabel_ = nullptr;
  lv_obj_t* summaryLoadLabel_ = nullptr;
  lv_obj_t* summaryCoachLabel_ = nullptr;
  lv_obj_t* summaryDetailLabel_ = nullptr;
  lv_obj_t* summaryCountdownLabel_ = nullptr;
  lv_obj_t* btnSummarySkip_ = nullptr;

  // Header
  lv_obj_t* topStateLabel_ = nullptr;
  lv_obj_t* topMachineLabel_ = nullptr;
  lv_obj_t* topUserLabel_ = nullptr;
  lv_obj_t* topDateLabel_ = nullptr;
  lv_obj_t* topTimeLabel_ = nullptr;
  lv_obj_t* topSyncLabel_ = nullptr;

  // Main display
  lv_obj_t* bigWeightLabel_ = nullptr;
  lv_obj_t* pinLoadTitleLabel_ = nullptr;
  lv_obj_t* recommendationTitleLabel_ = nullptr;
  lv_obj_t* suggestionLabel_ = nullptr;
  lv_obj_t* sessionTimerLabel_ = nullptr;
  lv_obj_t* repMainLabel_ = nullptr;
  lv_obj_t* setMainLabel_ = nullptr;
  lv_obj_t* romMainLabel_ = nullptr;
  lv_obj_t* velocityMainLabel_ = nullptr;
  lv_obj_t* qualityLabel_ = nullptr;
  lv_obj_t* speedCueLabel_ = nullptr;
  lv_obj_t* statusMainLabel_ = nullptr;
  lv_obj_t* historyMiniLabel_ = nullptr;
  lv_obj_t* sessionMetaLabel_ = nullptr;
  lv_obj_t* paceMainLabel_ = nullptr;
  lv_obj_t* motionLiveDot_ = nullptr;
  lv_obj_t* motionLiveHalo_ = nullptr;

  lv_obj_t* romBar_ = nullptr;
  lv_obj_t* setProgressBar_ = nullptr;
  lv_obj_t* calibrationBar_ = nullptr;
  lv_obj_t* metricsChart_ = nullptr;
  lv_chart_series_t* metricsSeries_ = nullptr;
  lv_chart_series_t* idealSeries_ = nullptr;
  lv_chart_series_t* idealSeriesUpper_ = nullptr;
  lv_chart_series_t* idealSeriesLower_ = nullptr;
  lv_obj_t* chartActualLegend_ = nullptr;
  lv_obj_t* chartIdealLegend_ = nullptr;
  lv_obj_t* romTargetBand_ = nullptr;
  lv_obj_t* romTargetBandLabel_ = nullptr;
  static constexpr uint16_t kMotionHistoryPoints = 512;
  static constexpr uint8_t kMotionGraphPoints = 120;
  float motionActualHistory_[kMotionHistoryPoints] = {};
  float motionIdealHistory_[kMotionHistoryPoints] = {};
  uint32_t motionHistoryTimeMs_[kMotionHistoryPoints] = {};
  float chartActualFiltered_ = 0.0f;
  lv_coord_t lastMotionDotX_ = -32768;
  lv_coord_t lastMotionDotY_ = -32768;
  bool lastMotionDotOnPath_ = false;
  uint16_t motionHistoryCount_ = 0;
  uint16_t motionHistoryHead_ = 0;
  uint32_t idealPhaseMs_ = 0;
  uint32_t idealPhaseLastTickMs_ = 0;
  uint32_t idealBottomStillSinceMs_ = 0;
  uint32_t idealPauseCandidateMs_ = 0;
  lv_obj_t* restOverlay_ = nullptr;
  lv_obj_t* restPanel_ = nullptr;
  lv_obj_t* restArc_ = nullptr;
  lv_obj_t* restCountdownLabel_ = nullptr;
  lv_obj_t* restTitleLabel_ = nullptr;
  lv_obj_t* restDetailLabel_ = nullptr;
  lv_obj_t* btnRestSkip_ = nullptr;
  lv_obj_t* userLoadingScreen_ = nullptr;
  lv_obj_t* userLoadingPanel_ = nullptr;
  lv_obj_t* userLoadingTitleLabel_ = nullptr;
  lv_obj_t* userLoadingStageLabel_ = nullptr;
  UiScreenMode userLoadingReturnScreen_ = UiScreenMode::Main;

  // Main buttons
  lv_obj_t* btnWeightMinus_ = nullptr;
  lv_obj_t* btnWeightPlus_ = nullptr;
  lv_obj_t* btnWeightMinusLarge_ = nullptr;
  lv_obj_t* btnWeightPlusLarge_ = nullptr;
  lv_obj_t* btnCalibrate_ = nullptr;
  lv_obj_t* btnTrain_ = nullptr;
  lv_obj_t* btnService_ = nullptr;
  lv_obj_t* btnAvatar_ = nullptr;
  bool autoRepContinuous_ = false;
  bool autoRepStopAfterCycle_ = false;
  bool sessionLogoutPending_ = false;
  bool finishRequestPending_ = false;
  bool finishRequestLogoutNow_ = false;
  bool autoFinishRequestPending_ = false;
  String autoFinishReason_;

  // Idle screen
  lv_obj_t* idleOverlay_ = nullptr;
  lv_obj_t* idlePanel_ = nullptr;
  lv_obj_t* idleBadgeLabel_ = nullptr;
  lv_obj_t* idleTitleLabel_ = nullptr;
  lv_obj_t* idleMachineLabel_ = nullptr;
  lv_obj_t* idlePromptLabel_ = nullptr;
  lv_obj_t* idleHintLabel_ = nullptr;
  lv_obj_t* userSwitchPrompt_ = nullptr;
  lv_obj_t* userSwitchPromptTitle_ = nullptr;
  lv_obj_t* userSwitchPromptBody_ = nullptr;
  lv_obj_t* btnUserSwitchConfirm_ = nullptr;
  lv_obj_t* btnUserSwitchCancel_ = nullptr;
  String pendingSwitchUid_;
  uint32_t idleOverlaySuppressedUntilMs_ = 0;
  bool idleStartupLock_ = true;
  uint32_t idealGraphStartMs_ = 0;
  bool sessionMotionTemplateLatched_ = false;
  uint16_t sessionTplRiseMs_ = 900;
  uint16_t sessionTplTopPauseMs_ = 250;
  uint16_t sessionTplLowerMs_ = 900;
  uint16_t sessionTplBottomPauseMs_ = 250;
  float encoderNoiseFloorPct_ = 0.0f;
  bool startupSelfTestDone_ = false;
  String startupSelfTestSummary_;
  TaskHandle_t syncTaskHandle_ = nullptr;
  SemaphoreHandle_t cloudMutex_ = nullptr;
  volatile bool syncWorkerStop_ = false;
  volatile bool syncWorkerForce_ = false;
  volatile bool syncWorkerBusy_ = false;
  TaskHandle_t uploadTransportTaskHandle_ = nullptr;
  volatile bool uploadTransportWorkerStop_ = false;
  volatile bool uploadTransportWorkerBusy_ = false;
  uint32_t uploadTransportLastIdleLogMs_ = 0;
  uint32_t uploadTransportLastLowHeapLogMs_ = 0;
  uint32_t syncWorkerLastServiceMs_ = 0;
  uint32_t perfFrameCount_ = 0;
  uint32_t perfDroppedFrames_ = 0;
  uint32_t perfLastFrameMs_ = 0;
  uint32_t perfLastChartMs_ = 0;
  uint32_t perfAvgFrameMsQ8_ = 0;
  uint32_t perfAvgChartMsQ8_ = 0;
  bool finishTransitionInProgress_ = false;
  String lastFinishedSessionId_;
  SessionHistoryRecord lastSummaryRecord_;
  bool lastCloudConnected_ = false;
  bool pendingCanonicalSessionSync_ = false;
  SessionHistoryRecord pendingCanonicalSessionRecord_;
  bool pendingSessionQueueEnqueue_ = false;
  SessionHistoryRecord pendingSessionQueueRecord_;
  bool pendingSessionDetailQueueEnqueue_ = false;
  bool pendingSessionDetailWaitLogged_ = false;
  SessionHistoryRecord pendingSessionDetailQueueRecord_;
  bool pendingWebDetailUpload_ = false;
  String pendingWebDetailWeekKey_;
  String pendingWebDetailDayKey_;
  String pendingWebDetailSessionPath_;
  uint8_t pendingWebDetailPhase_ = 0;  // 0 none, 1 core queued, 2 details queued, 3 repSets
  uint8_t pendingWebDetailNextSet_ = 1;
  uint16_t pendingWebDetailNextRep_ = 1;
  bool pendingWebDetailSplitSet_ = false;
  bool pendingWebDetailSetSummaryQueued_ = false;  // Used as split-set header queued flag.
  bool pendingWebRootUploaded_ = false;
  String pendingWebRootPath_;
  bool pendingWebCoreBundleRequired_ = false;
  bool pendingWebCoreBundleUploaded_ = false;
  String pendingWebCoreBundlePath_;
  uint16_t pendingWebCoreBundlePayloadBytes_ = 0;
  uint32_t pendingWebDetailPauseLogMs_ = 0;
  uint16_t pendingWebDetailPauseQueueCount_ = 0xFFFF;
  uint8_t pendingWebDetailDeferSet_ = 0;
  uint16_t pendingWebDetailDeferQueueCount_ = 0xFFFF;
  uint32_t pendingWebDetailDeferLogMs_ = 0;
  bool uploadYieldAfterBatchEnqueue_ = false;
  bool fatQueueReady_ = false;
  bool summaryWaitingForSync_ = false;
  uint32_t summarySyncWaitStartedMs_ = 0;
  String lastSummarySyncStatus_;
  String summaryUploadStageText_;
  bool pendingScanReconcile_ = false;
  uint32_t pendingScanReconcileEarliestMs_ = 0;
  uint32_t pendingScanReconcileStartedMs_ = 0;
  uint8_t pendingScanReconcileStep_ = 0;
  bool userLoading_ = false;
  bool bootMachineRestorePending_ = false;
  LogLevel logLevel_ = LogLevel::Normal;
  uint32_t lastHeartbeatLogMs_ = 0;
  uint32_t lastSyncGateDiagLogMs_ = 0;
  uint32_t lastSyncWorkerDiagLogMs_ = 0;
  UploadMemoryMode lastUploadMemoryMode_ = UploadMemoryMode::Normal;
  uint32_t lastUploadMemoryModeLogMs_ = 0;
  bool lowMemoryUploadUiFreed_ = false;
  bool graphLayoutLogged_ = false;
  String lastHeapDelayPath_;
  uint16_t lastHeapDelayPayloadBytes_ = 0;
  uint8_t repeatedHeapDelayCount_ = 0;
  State lastLoggedState_ = State::Idle;
  SessionStage lastLoggedStage_ = SessionStage::Idle;
  static constexpr uint8_t kLogTraceSize = 30;
  String logTrace_[kLogTraceSize];
  uint8_t logTraceHead_ = 0;
  uint8_t logTraceCount_ = 0;

  // Guided calibration screen
  lv_obj_t* calibrationScreen_ = nullptr;
  lv_obj_t* calibrationVisualPanel_ = nullptr;
  lv_obj_t* calibrationInstructionPanel_ = nullptr;
  lv_obj_t* calibrationHeaderLabel_ = nullptr;
  lv_obj_t* calibrationMachineLabel_ = nullptr;
  lv_obj_t* calibrationStepLabel_ = nullptr;
  lv_obj_t* calibrationInstructionLabel_ = nullptr;
  lv_obj_t* calibrationLoadLabel_ = nullptr;
  lv_obj_t* calibrationMetricLabel_ = nullptr;
  lv_obj_t* calibrationFeedbackLabel_ = nullptr;
  lv_obj_t* calibrationRomBar_ = nullptr;
  lv_obj_t* btnCalibrationWeightMinusLarge_ = nullptr;
  lv_obj_t* btnCalibrationWeightMinus_ = nullptr;
  lv_obj_t* btnCalibrationWeightPlus_ = nullptr;
  lv_obj_t* btnCalibrationWeightPlusLarge_ = nullptr;
  lv_obj_t* btnCalibrationPrimary_ = nullptr;
  lv_obj_t* btnCalibrationSecondary_ = nullptr;
  lv_obj_t* btnCalibrationCancel_ = nullptr;
  bool calibrationLayoutLogged_ = false;
  bool calibrationPinButtonsEnabled_ = true;

  // Calibration gate screen
  lv_obj_t* calibrationGateScreen_ = nullptr;
  lv_obj_t* calibrationGatePanel_ = nullptr;
  lv_obj_t* calibrationGateTitleLabel_ = nullptr;
  lv_obj_t* calibrationGateBodyLabel_ = nullptr;
  lv_obj_t* calibrationGateMachineLabel_ = nullptr;
  lv_obj_t* calibrationGateHintLabel_ = nullptr;
  lv_obj_t* btnCalibrationGateSkip_ = nullptr;
  lv_obj_t* btnCalibrationGateCalibrate_ = nullptr;

  // Service/debug screen
  lv_obj_t* debugModal_ = nullptr;
  lv_obj_t* debugPanel_ = nullptr;
  lv_obj_t* debugStatusLabel_ = nullptr;
  lv_obj_t* debugHardwareLabel_ = nullptr;
  lv_obj_t* machineDropdown_ = nullptr;
  lv_obj_t* romSlider_ = nullptr;
  lv_obj_t* btnCloseDebug_ = nullptr;
  lv_obj_t* btnUser1_ = nullptr;
  lv_obj_t* btnUser2_ = nullptr;
  lv_obj_t* btnNewUser_ = nullptr;
  lv_obj_t* btnAnonymous_ = nullptr;
  lv_obj_t* btnAutoRep_ = nullptr;
  lv_obj_t* btnResetMotion_ = nullptr;
  lv_obj_t* btnJumpTop_ = nullptr;
  lv_obj_t* btnJumpBottom_ = nullptr;
  lv_obj_t* btnSyncCloud_ = nullptr;
  lv_obj_t* btnToggleSensorMode_ = nullptr;
  lv_obj_t* btnLogLevel_ = nullptr;
  lv_obj_t* btnEncoderZero_ = nullptr;
  lv_obj_t* btnEncoderFull_ = nullptr;
  lv_obj_t* btnEncoderApply_ = nullptr;
  lv_obj_t* btnEncoderReset_ = nullptr;
  lv_obj_t* btnUserRomBottom_ = nullptr;
  lv_obj_t* btnUserRomTop_ = nullptr;
  lv_obj_t* btnUserRomApply_ = nullptr;
  lv_obj_t* btnDebugCalibrate_ = nullptr;

  // Profile screen
  lv_obj_t* profileScreen_ = nullptr;
  lv_obj_t* profilePanel_ = nullptr;
  lv_obj_t* profileTitleLabel_ = nullptr;
  lv_obj_t* profileUidLabel_ = nullptr;
  lv_obj_t* profileNameValueLabel_ = nullptr;
  lv_obj_t* btnProfileEditName_ = nullptr;
  lv_obj_t* profileAgeValueLabel_ = nullptr;
  lv_obj_t* profileWeightValueLabel_ = nullptr;
  lv_obj_t* profileHeightValueLabel_ = nullptr;
  lv_obj_t* profileGoalValueLabel_ = nullptr;
  lv_obj_t* profileGenderValueLabel_ = nullptr;
  lv_obj_t* btnProfileNamePrev_ = nullptr;
  lv_obj_t* btnProfileNameNext_ = nullptr;
  lv_obj_t* btnProfileAgeMinus_ = nullptr;
  lv_obj_t* btnProfileAgePlus_ = nullptr;
  lv_obj_t* btnProfileWeightMinus_ = nullptr;
  lv_obj_t* btnProfileWeightPlus_ = nullptr;
  lv_obj_t* btnProfileHeightMinus_ = nullptr;
  lv_obj_t* btnProfileHeightPlus_ = nullptr;
  lv_obj_t* btnProfileGoalPrev_ = nullptr;
  lv_obj_t* btnProfileGoalNext_ = nullptr;
  lv_obj_t* btnProfileGenderPrev_ = nullptr;
  lv_obj_t* btnProfileGenderNext_ = nullptr;
  lv_obj_t* btnProfileSave_ = nullptr;
  lv_obj_t* btnProfileCancel_ = nullptr;
  lv_obj_t* profileNameTa_ = nullptr;
  lv_obj_t* profileKeyboard_ = nullptr;

  lv_timer_t* tickTimer_ = nullptr;

  void beginCore();
  void initStyles();
  void buildUi();
  void buildHeader(lv_obj_t* parent);
  void buildMainPanels(lv_obj_t* parent);
  void buildBottomControls(lv_obj_t* parent);
  void buildRestOverlay(lv_obj_t* parent);
  void buildUserLoadingPopup();
  void buildIdleOverlay(lv_obj_t* parent);
  void buildUserSwitchPrompt(lv_obj_t* parent);
  void buildSummaryScreen(lv_obj_t* parent);
  void buildCalibrationScreen(lv_obj_t* parent);
  void buildCalibrationGateScreen(lv_obj_t* parent);
  void buildDebugModal(lv_obj_t* parent);
  void buildProfileScreen(lv_obj_t* parent);
  void destroyDebugModalIfPresent();
  void destroyIdleOverlayIfPresent();
  void destroyProfileScreenIfPresent();
  void destroyCalibrationUiIfPresent();
  void clearCalibrationUiPointers();
  void destroyCalibrationUiForCancel(const char* reason = "calibration_cancel");
  void destroyCalibrationGateScreenIfPresent();
  void destroyUserSwitchPromptIfPresent();
  void freeOptionalUiForMemory(const char* reason, bool keepSummary = true);
  void freeOptionalUiForUpload(const char* reason, bool keepSummary = true);

  lv_obj_t* createPanel(lv_obj_t* parent, bool dark = false);
  lv_obj_t* createButton(lv_obj_t* parent, const char* text, int kind = 0);
  lv_obj_t* createValueTile(lv_obj_t* parent, const char* title, lv_obj_t** valueOut);
  bool isValidLvObj(lv_obj_t* obj) const;
  lv_obj_t* getButtonLabelIfValid(lv_obj_t* btn) const;
  void setButtonTextIfValid(lv_obj_t* btn, const char* text);
  void adjustCurrentLoad(float deltaKg, const char* source);
  bool acceptButtonAction(const char* action, uint32_t debounceMs);
  void showUserLoadingPopup(const String& uid, const char* stage);
  void updateUserLoadingPopupStage(const String& uid, const char* stage);
  void hideUserLoadingPopup(const String& uid, const char* result);

  void refreshUi();
  void refreshTopBar();
  void refreshMainStats(uint32_t nowMs);
  void refreshStatusArea(uint32_t nowMs);
  void refreshDebugPanel(uint32_t nowMs);
  void updateMotionLiveDot(uint32_t nowMs);
  bool getMotionChartPlotRect(lv_area_t& outRect) const;
  lv_coord_t mapRomPercentToChartY(float romPct) const;
  void updateIdealGuideLine(uint32_t nowMs);
  void renderMotionChartFrame(uint32_t nowMs);
  void appendMotionGraphSample(float actualPercent, float idealPercent, uint32_t nowMs);
  void resetMotionGraph();
  void refreshIdleOverlay(uint32_t nowMs);
  void refreshSummaryScreen(uint32_t nowMs);
  void refreshCalibrationScreen(uint32_t nowMs);
  void refreshRestOverlay(uint32_t nowMs);
  void refreshCalibrationGateScreen(uint32_t nowMs);
  void refreshProfileScreen(uint32_t nowMs);
  bool shouldShowIdleOverlay(uint32_t nowMs) const;
  bool isRestCountdownActive(uint32_t nowMs, uint32_t* remainingMs = nullptr) const;
  bool isMotionOnPath(float actualPercent, float idealPercent) const;
  SessionStage getSessionStage(uint32_t nowMs) const;
  bool canStartTrainingNow(uint32_t nowMs) const;
  bool canFinishNow(uint32_t nowMs) const;
  String buildPathStatusText(uint32_t nowMs);
  void wakeIdleOverlay(uint32_t nowMs, const String& reason = "");
  void markUserActivity(uint32_t nowMs);
  void showUiScreen(UiScreenMode mode);
  void openUserSwitchPrompt(const String& uid);
  void closeUserSwitchPrompt();

  void updateRuntime();
  void updateFromMotionInput(uint32_t nowMs);
  void updateAutoMotion();
  void syncRomDebugSliderIfPresent();
  void handleRep(const RepMetrics& rep, uint32_t nowMs);
  void processHardwareInputs(uint32_t nowMs);
  void updateCloudSync(uint32_t nowMs, bool forcePendingUploads);
  void updateCloudSyncWorker(uint32_t nowMs, bool forcePendingUploads);
  void startSyncWorker();
  void startUploadTransportWorker();
  void requestCloudSync(bool force);
  bool retryPendingUploads(uint32_t nowMs, bool force);
  bool tryResplitQueuedRepSetPatch(const PendingUploadRecord& upload, UploadMemoryMode mode);
  bool syncDeviceIdentityToCloud(uint32_t nowMs, bool force);
  bool refreshDeviceCalibrationFromCloud();
  bool isSessionUploadBusy(uint32_t nowMs,
                           const char** reasonOut = nullptr,
                           uint16_t* queueCountOut = nullptr) const;
  UploadMemoryMode getUploadMemoryMode(uint32_t& internalHeapOut, uint32_t& largestBlockOut) const;
  const char* uploadMemoryModeText(UploadMemoryMode mode) const;
  bool shouldSuppressOptionalCloudRead(const char* kind, uint32_t nowMs) const;
  bool shouldSkipRecentCloudRead(const char* kind, const String& uid, uint32_t nowMs);
  void resetPendingWebDetailUpload();
  void pumpPendingWebDetailUpload();
  void syncActiveUserToCloud();
  void syncActiveCalibrationToCloud();
  bool queueSessionUpload(const SessionHistoryRecord& record, bool includeHeavyDetails);
  void refreshActiveUserFromCloud(const String& uid);
  void refreshActiveCalibrationFromCloud();
  bool mergeActiveUserFromCloud(const UserProfile& cloudProfile);
  bool mergeActiveCalibrationFromCloud(const UserMachineCalibration& cloudCalibration);
  bool reconcileCloudStateForScan();
  void applyMachineSensorCalibration();
  MachineCloudConfig buildCurrentMachineConfig() const;
  bool pushCurrentMachineConfigToCloud();
  void captureEncoderZero();
  void captureEncoderFull();
  void applyEncoderCalibration();
  void resetEncoderCalibration();
  void captureUserRomBottom();
  void captureUserRomTop();
  void applyUserRomCalibration();

  void startCalibration(const char* source);
  void transitionCalibrationState(CalibrationFlowState nextState, const String& reason = "");
  void cancelCalibration(const String& reason);
  void updateCalibrationFromButtonPress(bool secondaryAction);
  void processCalibrationRep(const RepMetrics& rep, uint32_t nowMs);
  float computeFirstCalibrationWeightKg();
  float roundWeightToMachineIncrement(float kg) const;
  bool deriveMotionTargetsForActiveMachine(MotionTargetConfig& outConfig);
  bool deriveMotionTargetsForActiveMachine(MotionTargetConfig& outConfig, TrainingGoal requestedGoal,
                                           bool allowTestGoal, const char* source);
  TrainingGoal normalizeGoalId(TrainingGoal rawGoal, const char* source, bool allowTestGoal);
  bool isDebugGoalSelectionEnabled() const;
  TrainingGoal cycleProfileGoal(TrainingGoal current, int direction, bool allowTestGoal) const;
  void applySessionRecommendationFromSummary(const SessionHistoryRecord& record);
  ResolvedRecommendation resolveRecommendedLoadForActiveUserMachine();
  void refreshResolvedRecommendation(bool applyToCurrentLoad, const char* source);
  void updateSelectedWeightFromRecommendation();
  void startTraining(bool forceWithoutCalibration = false);
  void finishTraining(const String& reason, bool logoutNow = false);
  void sanitizeSessionForUpload(SessionHistoryRecord& record) const;
  void activateUserByUid(const String& uid, bool createIfMissing, bool reconcileCloud = true);
  void activateAnonymousMode();
  void refreshActiveCalibration();
  void configureRepDetectorThresholds();
  void setMachineById(const String& machineId);
  void logoutActiveUser(const String& reason, bool goIdleScreen = true);
  void maybeAutoLogoutAfterSession(uint32_t nowMs);

  const GoalRecommendation* getRecommendation() const;
  uint8_t getTargetSets() const;
  uint8_t getTargetReps() const;
  const char* stateToText() const;
  String buildUserText() const;
  String buildMachineText() const;
  String buildSuggestionText() const;
  String buildSessionTimerText(uint32_t nowMs) const;
  float buildIdealMotionPercent(uint32_t nowMs);
  String buildRepPhaseText() const;
  String buildSpeedSummaryText() const;
  String buildRestCountdownText(uint32_t nowMs) const;
  String buildWorkoutDetailText(uint32_t nowMs) const;
  String buildSessionMetaText(uint32_t nowMs) const;
  String buildLiveFeedbackText(uint32_t nowMs) const;
  String buildStatusText() const;
  String buildQualityText() const;
  String buildInvalidReason(const RepMetrics& rep) const;
  float getActiveRomPercent(float* rawOut = nullptr, const char** sourceOut = nullptr) const;
  float buildDisplayedLiveRomPercent() const;
  String buildSessionId() const;
  String buildTimeText() const;
  String buildDebugStatusText() const;
  String buildHardwareStatusText() const;

  void setStatusMessage(const String& message);
  void saveUsers();
  void pulseTo(float romTarget);
  void syncCloudNow();
  void openDebugMenu();
  void closeDebugMenu();
  void openProfileEditor(bool forceCreateMode);
  void closeProfileEditor(bool discardChanges);
  bool saveProfileEditor();
  const char* goalDisplayName(TrainingGoal goal) const;
  const char* genderDisplayName(UserGender gender) const;
  const char* logLevelText() const;
  void logEvent(const char* tag, const String& message, LogLevel level = LogLevel::Normal);
  void pushLogTrace(const String& line);
  void dumpLogTrace(const char* reason);
  void logStateIfChanged(uint32_t nowMs);
  void logHeartbeat(uint32_t nowMs);
  const char* sessionStageToText(SessionStage stage) const;
  const char* uiScreenToText(UiScreenMode mode) const;
  String nextProfileName(const String& current, int direction) const;

  static void onButtonEvent(lv_event_t* event);
  static void onSliderEvent(lv_event_t* event);
  static void onMachineChanged(lv_event_t* event);
  static void onIdleOverlayEvent(lv_event_t* event);
  static void onMotionChartDraw(lv_event_t* event);
  static void onTick(lv_timer_t* timer);
  static void syncTaskEntry(void* arg);
  static void uploadTransportTaskEntry(void* arg);
};
