#pragma once

#include "language/base.h"
#include "serde/serde.h"

namespace eventide::language::proto {

enum class SemanticTokenTypesEnum {
    namespace_,
    type,
    class_,
    enum_,
    interface,
    struct_,
    typeParameter,
    parameter,
    variable,
    property,
    enumMember,
    event,
    function,
    method,
    macro,
    keyword,
    modifier,
    comment,
    string,
    number,
    regexp,
    operator_,
    decorator,
};
using SemanticTokenTypes = std::variant<SemanticTokenTypesEnum, std::string>;

enum class SemanticTokenModifiersEnum {
    declaration,
    definition,
    readonly,
    static_,
    deprecated,
    abstract,
    async,
    modification,
    documentation,
    defaultLibrary,
};
using SemanticTokenModifiers = std::variant<SemanticTokenModifiersEnum, std::string>;

enum class DocumentDiagnosticReportKind {
    Full,
    Unchanged,
};

enum class ErrorCodes : std::int32_t {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,
};

enum class LSPErrorCodes : std::int32_t {
    RequestFailed = -32803,
    ServerCancelled = -32802,
    ContentModified = -32801,
    RequestCancelled = -32800,
};

enum class FoldingRangeKindEnum {
    Comment,
    Imports,
    Region,
};
using FoldingRangeKind = std::variant<FoldingRangeKindEnum, std::string>;

enum class SymbolKind : std::uint32_t {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26,
};

enum class SymbolTag : std::uint32_t {
    Deprecated = 1,
};

enum class UniquenessLevel {
    document,
    project,
    group,
    scheme,
    global,
};

enum class MonikerKind {
    import,
    export_,
    local,
};

enum class InlayHintKind : std::uint32_t {
    Type = 1,
    Parameter = 2,
};

enum class MessageType : std::uint32_t {
    Error = 1,
    Warning = 2,
    Info = 3,
    Log = 4,
    Debug = 5,
};

enum class TextDocumentSyncKind : std::uint32_t {
    None = 0,
    Full = 1,
    Incremental = 2,
};

enum class TextDocumentSaveReason : std::uint32_t {
    Manual = 1,
    AfterDelay = 2,
    FocusOut = 3,
};

enum class CompletionItemKind : std::uint32_t {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

enum class CompletionItemTag : std::uint32_t {
    Deprecated = 1,
};

enum class InsertTextFormat : std::uint32_t {
    PlainText = 1,
    Snippet = 2,
};

enum class InsertTextMode : std::uint32_t {
    asIs = 1,
    adjustIndentation = 2,
};

enum class DocumentHighlightKind : std::uint32_t {
    Text = 1,
    Read = 2,
    Write = 3,
};

enum class CodeActionKindEnum {
    Empty,
    QuickFix,
    Refactor,
    RefactorExtract,
    RefactorInline,
    RefactorRewrite,
    Source,
    SourceOrganizeImports,
    SourceFixAll,
};
using CodeActionKind = std::variant<CodeActionKindEnum, std::string>;

enum class TraceValues {
    Off,
    Messages,
    Verbose,
};

enum class MarkupKind {
    PlainText,
    Markdown,
};

enum class InlineCompletionTriggerKind : std::uint32_t {
    Invoked = 0,
    Automatic = 1,
};

enum class PositionEncodingKindEnum {
    UTF8,
    UTF16,
    UTF32,
};
using PositionEncodingKind = std::variant<PositionEncodingKindEnum, std::string>;

enum class FileChangeType : std::uint32_t {
    Created = 1,
    Changed = 2,
    Deleted = 3,
};

enum class WatchKind : std::uint32_t {
    Create = 1,
    Change = 2,
    Delete = 4,
};

enum class DiagnosticSeverity : std::uint32_t {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4,
};

enum class DiagnosticTag : std::uint32_t {
    Unnecessary = 1,
    Deprecated = 2,
};

enum class CompletionTriggerKind : std::uint32_t {
    Invoked = 1,
    TriggerCharacter = 2,
    TriggerForIncompleteCompletions = 3,
};

enum class SignatureHelpTriggerKind : std::uint32_t {
    Invoked = 1,
    TriggerCharacter = 2,
    ContentChange = 3,
};

enum class CodeActionTriggerKind : std::uint32_t {
    Invoked = 1,
    Automatic = 2,
};

enum class FileOperationPatternKind {
    file,
    folder,
};

enum class NotebookCellKind : std::uint32_t {
    Markup = 1,
    Code = 2,
};

enum class ResourceOperationKind {
    Create,
    Rename,
    Delete,
};

enum class FailureHandlingKind {
    Abort,
    Transactional,
    TextOnlyTransactional,
    Undo,
};

enum class PrepareSupportDefaultBehavior : std::uint32_t {
    Identifier = 1,
};

enum class TokenFormat {
    Relative,
};

struct ReferenceClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct CreateFileOptions {
    std::optional<bool> overwrite;
    std::optional<bool> ignoreIfExists;
};

struct InlineValueWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct LspLiteral19 {
    std::optional<std::vector<SymbolKind>> valueSet;
};

struct DeleteFileOptions {
    std::optional<bool> recursive;
    std::optional<bool> ignoreIfNotExists;
};

struct Command {
    std::string title;
    std::string command;
    std::optional<std::vector<LSPAny>> arguments;
};

struct DiagnosticServerCancellationData {
    bool retriggerRequest;
};

struct InitializeError {
    bool retry;
};

struct LspLiteral44 {
    std::optional<std::string> language;
    std::string scheme;
    std::optional<std::string> pattern;
};

struct WorkDoneProgressEnd {
    rfl::Literal<"end"> kind;
    std::optional<std::string> message;
};

struct MarkdownClientCapabilities {
    std::string parser;
    std::optional<std::string> version;
    std::optional<std::vector<std::string>> allowedTags;
};

struct LspLiteral42 {
    std::string language;
    std::string value;
};

using MarkedString = std::variant<std::string, LspLiteral42>;

struct NotebookDocumentIdentifier {
    URI uri;
};

struct DidSaveNotebookDocumentParams {
    NotebookDocumentIdentifier notebookDocument;
};

struct TypeHierarchyClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral48 {
    std::optional<std::string> notebookType;
    std::optional<std::string> scheme;
    std::string pattern;
};

struct WorkDoneProgressBegin {
    rfl::Literal<"begin"> kind;
    std::string title;
    std::optional<bool> cancellable;
    std::optional<std::string> message;
    std::optional<std::uint32_t> percentage;
};

struct FileDelete {
    std::string uri;
};

struct DeleteFilesParams {
    std::vector<FileDelete> files;
};

struct WorkspaceFoldersServerCapabilities {
    std::optional<bool> supported;
    std::optional<std::variant<std::string, bool>> changeNotifications;
};

struct ExecuteCommandClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral24 {
    std::vector<std::string> properties;
};

struct InlayHintClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral24> resolveSupport;
};

struct DocumentOnTypeFormattingOptions {
    std::string firstTriggerCharacter;
    std::optional<std::vector<std::string>> moreTriggerCharacter;
};

struct ExecutionSummary {
    std::uint32_t executionOrder;
    std::optional<bool> success;
};

struct NotebookCell {
    NotebookCellKind kind;
    DocumentUri document;
    std::optional<LSPObject> metadata;
    std::optional<ExecutionSummary> executionSummary;
};

struct NotebookCellArrayChange {
    std::uint32_t start;
    std::uint32_t deleteCount;
    std::optional<std::vector<NotebookCell>> cells;
};

struct NotebookDocument {
    URI uri;
    std::string notebookType;
    std::int32_t version;
    std::optional<LSPObject> metadata;
    std::vector<NotebookCell> cells;
};

struct PreviousResultId {
    DocumentUri uri;
    std::string value;
};

struct LspLiteral43 {
    std::string language;
    std::optional<std::string> scheme;
    std::optional<std::string> pattern;
};

struct FileOperationPatternOptions {
    std::optional<bool> ignoreCase;
};

struct FileOperationPattern {
    std::string glob;
    std::optional<FileOperationPatternKind> matches;
    std::optional<FileOperationPatternOptions> options;
};

struct FileOperationFilter {
    std::optional<std::string> scheme;
    FileOperationPattern pattern;
};

struct FileOperationRegistrationOptions {
    std::vector<FileOperationFilter> filters;
};

struct FileOperationOptions {
    std::optional<FileOperationRegistrationOptions> didCreate;
    std::optional<FileOperationRegistrationOptions> willCreate;
    std::optional<FileOperationRegistrationOptions> didRename;
    std::optional<FileOperationRegistrationOptions> willRename;
    std::optional<FileOperationRegistrationOptions> didDelete;
    std::optional<FileOperationRegistrationOptions> willDelete;
};

struct LspLiteral12 {
    std::optional<WorkspaceFoldersServerCapabilities> workspaceFolders;
    std::optional<FileOperationOptions> fileOperations;
};

struct LspLiteral17 {
    bool cancel;
    std::vector<std::string> retryOnContentModified;
};

struct LspLiteral47 {
    std::optional<std::string> notebookType;
    std::string scheme;
    std::optional<std::string> pattern;
};

struct LspLiteral29 {
    std::optional<bool> labelOffsetSupport;
};

struct LspLiteral28 {
    std::optional<std::vector<MarkupKind>> documentationFormat;
    std::optional<LspLiteral29> parameterInformation;
    std::optional<bool> activeParameterSupport;
};

struct SignatureHelpClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral28> signatureInformation;
    std::optional<bool> contextSupport;
};

struct LspLiteral13 {
    std::optional<bool> labelDetailsSupport;
};

struct DidChangeConfigurationClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct DocumentRangeFormattingClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> rangesSupport;
};

struct LspLiteral37 {
    std::optional<bool> additionalPropertiesSupport;
};

struct ShowMessageRequestClientCapabilities {
    std::optional<LspLiteral37> messageActionItem;
};

struct LspLiteral36 {
    std::optional<bool> delta;
};

struct ReferenceContext {
    bool includeDeclaration;
};

struct LspLiteral23 {
    std::vector<CompletionItemTag> valueSet;
};

struct LspLiteral7 {
    std::optional<bool> delta;
};

struct SelectionRangeClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LogMessageParams {
    MessageType type;
    std::string message;
};

struct RenameClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> prepareSupport;
    std::optional<PrepareSupportDefaultBehavior> prepareSupportDefaultBehavior;
    std::optional<bool> honorsChangeAnnotations;
};

struct LspLiteral4 {
    std::string reason;
};

struct NotebookDocumentSyncClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> executionSummarySupport;
};

struct NotebookDocumentClientCapabilities {
    NotebookDocumentSyncClientCapabilities synchronization;
};

struct SaveOptions {
    std::optional<bool> includeText;
};

struct TextDocumentSyncOptions {
    std::optional<bool> openClose;
    std::optional<TextDocumentSyncKind> change;
    std::optional<bool> willSave;
    std::optional<bool> willSaveWaitUntil;
    std::optional<std::variant<bool, SaveOptions>> save;
};

struct LspLiteral15 {
    std::string language;
};

struct SemanticTokens {
    std::optional<std::string> resultId;
    std::vector<std::uint32_t> data;
};

struct FormattingOptions {
    std::uint32_t tabSize;
    bool insertSpaces;
    std::optional<bool> trimTrailingWhitespace;
    std::optional<bool> insertFinalNewline;
    std::optional<bool> trimFinalNewlines;
};

struct DiagnosticWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct SemanticTokensWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

using Pattern = std::string;

struct MonikerClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral18 {
    std::optional<bool> groupsOnLabel;
};

struct WorkspaceEditClientCapabilities {
    std::optional<bool> documentChanges;
    std::optional<std::vector<ResourceOperationKind>> resourceOperations;
    std::optional<FailureHandlingKind> failureHandling;
    std::optional<bool> normalizesLineEndings;
    std::optional<LspLiteral18> changeAnnotationSupport;
};

struct SemanticTokensPartialResult {
    std::vector<std::uint32_t> data;
};

struct ShowDocumentClientCapabilities {
    bool support;
};

struct WindowClientCapabilities {
    std::optional<bool> workDoneProgress;
    std::optional<ShowMessageRequestClientCapabilities> showMessage;
    std::optional<ShowDocumentClientCapabilities> showDocument;
};

struct ConfigurationItem {
    std::optional<URI> scopeUri;
    std::optional<std::string> section;
};

struct ConfigurationParams {
    std::vector<ConfigurationItem> items;
};

struct CodeDescription {
    URI href;
};

struct WorkspaceFolder {
    URI uri;
    std::string name;
};

struct RelativePattern {
    std::variant<WorkspaceFolder, URI> baseUri;
    Pattern pattern;
};

using GlobPattern = std::variant<Pattern, RelativePattern>;

struct FileSystemWatcher {
    GlobPattern globPattern;
    std::optional<WatchKind> kind;
};

struct DidChangeWatchedFilesRegistrationOptions {
    std::vector<FileSystemWatcher> watchers;
};

struct WorkspaceFoldersChangeEvent {
    std::vector<WorkspaceFolder> added;
    std::vector<WorkspaceFolder> removed;
};

struct DidChangeWorkspaceFoldersParams {
    WorkspaceFoldersChangeEvent event;
};

struct WorkspaceFoldersInitializeParams {
    std::optional<std::vector<WorkspaceFolder>> workspaceFolders;
};

struct LspLiteral26 {
    std::optional<std::vector<CompletionItemKind>> valueSet;
};

struct FileOperationClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> didCreate;
    std::optional<bool> willCreate;
    std::optional<bool> didRename;
    std::optional<bool> willRename;
    std::optional<bool> didDelete;
    std::optional<bool> willDelete;
};

struct DidChangeWatchedFilesClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> relativePatternSupport;
};

struct ChangeAnnotation {
    std::string label;
    std::optional<bool> needsConfirmation;
    std::optional<std::string> description;
};

struct ApplyWorkspaceEditResult {
    bool applied;
    std::optional<std::string> failureReason;
    std::optional<std::uint32_t> failedChange;
};

struct Registration {
    std::string id;
    std::string method;
    std::optional<LSPAny> registerOptions;
};

struct RegistrationParams {
    std::vector<Registration> registrations;
};

struct LspLiteral31 {
    std::vector<CodeActionKind> valueSet;
};

struct LspLiteral30 {
    LspLiteral31 codeActionKind;
};

struct CodeActionClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral30> codeActionLiteralSupport;
    std::optional<bool> isPreferredSupport;
    std::optional<bool> disabledSupport;
    std::optional<bool> dataSupport;
    std::optional<LspLiteral24> resolveSupport;
    std::optional<bool> honorsChangeAnnotations;
};

struct DocumentFormattingClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct Position {
    std::uint32_t line;
    std::uint32_t character;
};

struct Range {
    Position start;
    Position end;
};

struct InsertReplaceEdit {
    std::string newText;
    Range insert;
    Range replace;
};

struct CodeLens {
    Range range;
    std::optional<Command> command;
    std::optional<LSPAny> data;
};

struct SelectionRange {
    Range range;
    std::optional<std::shared_ptr<SelectionRange>> parent;
};

struct LspLiteral3 {
    Range insert;
    Range replace;
};

struct LspLiteral2 {
    std::optional<std::vector<std::string>> commitCharacters;
    std::optional<std::variant<Range, LspLiteral3>> editRange;
    std::optional<InsertTextFormat> insertTextFormat;
    std::optional<InsertTextMode> insertTextMode;
    std::optional<LSPAny> data;
};

struct LinkedEditingRanges {
    std::vector<Range> ranges;
    std::optional<std::string> wordPattern;
};

struct InlineValueText {
    Range range;
    std::string text;
};

struct LspLiteral40 {
    Range range;
    std::optional<std::uint32_t> rangeLength;
    std::string text;
};

struct InlineValueVariableLookup {
    Range range;
    std::optional<std::string> variableName;
    bool caseSensitiveLookup;
};

struct CallHierarchyItem {
    std::string name;
    SymbolKind kind;
    std::optional<std::vector<SymbolTag>> tags;
    std::optional<std::string> detail;
    DocumentUri uri;
    Range range;
    Range selectionRange;
    std::optional<LSPAny> data;
};

struct CallHierarchyOutgoingCall {
    CallHierarchyItem to;
    std::vector<Range> fromRanges;
};

struct CallHierarchyIncomingCall {
    CallHierarchyItem from;
    std::vector<Range> fromRanges;
};

struct DocumentLink {
    Range range;
    std::optional<URI> target;
    std::optional<std::string> tooltip;
    std::optional<LSPAny> data;
};

struct InlineValueEvaluatableExpression {
    Range range;
    std::optional<std::string> expression;
};

using InlineValue =
    std::variant<InlineValueText, InlineValueVariableLookup, InlineValueEvaluatableExpression>;

struct ShowDocumentParams {
    URI uri;
    std::optional<bool> external;
    std::optional<bool> takeFocus;
    std::optional<Range> selection;
};

struct DocumentHighlight {
    Range range;
    std::optional<DocumentHighlightKind> kind;
};

struct TextEdit {
    Range range;
    std::string newText;
};

struct ColorPresentation {
    std::string label;
    std::optional<TextEdit> textEdit;
    std::optional<std::vector<TextEdit>> additionalTextEdits;
};

struct SelectedCompletionInfo {
    Range range;
    std::string text;
};

struct InlineCompletionContext {
    InlineCompletionTriggerKind triggerKind;
    std::optional<SelectedCompletionInfo> selectedCompletionInfo;
};

struct DocumentSymbol {
    std::string name;
    std::optional<std::string> detail;
    SymbolKind kind;
    std::optional<std::vector<SymbolTag>> tags;
    std::optional<bool> deprecated;
    Range range;
    Range selectionRange;
    std::optional<std::vector<DocumentSymbol>> children;
};

struct LspLiteral38 {
    Range range;
    std::string placeholder;
};

struct LocationLink {
    std::optional<Range> originSelectionRange;
    DocumentUri targetUri;
    Range targetRange;
    Range targetSelectionRange;
};

using DeclarationLink = LocationLink;

using DefinitionLink = LocationLink;

struct TypeHierarchyItem {
    std::string name;
    SymbolKind kind;
    std::optional<std::vector<SymbolTag>> tags;
    std::optional<std::string> detail;
    DocumentUri uri;
    Range range;
    Range selectionRange;
    std::optional<LSPAny> data;
};

struct InlineValueContext {
    std::int32_t frameId;
    Range stoppedLocation;
};

struct Location {
    DocumentUri uri;
    Range range;
};

struct DiagnosticRelatedInformation {
    Location location;
    std::string message;
};

struct Diagnostic {
    Range range;
    std::optional<DiagnosticSeverity> severity;
    std::optional<std::variant<std::int32_t, std::string>> code;
    std::optional<CodeDescription> codeDescription;
    std::optional<std::string> source;
    std::string message;
    std::optional<std::vector<DiagnosticTag>> tags;
    std::optional<std::vector<DiagnosticRelatedInformation>> relatedInformation;
    std::optional<LSPAny> data;
};

struct CodeActionContext {
    std::vector<Diagnostic> diagnostics;
    std::optional<std::vector<CodeActionKind>> only;
    std::optional<CodeActionTriggerKind> triggerKind;
};

struct PublishDiagnosticsParams {
    DocumentUri uri;
    std::optional<std::int32_t> version;
    std::vector<Diagnostic> diagnostics;
};

struct FullDocumentDiagnosticReport {
    rfl::Literal<"full"> kind;
    std::optional<std::string> resultId;
    std::vector<Diagnostic> items;
};

struct WorkspaceFullDocumentDiagnosticReport {
    rfl::Literal<"full"> kind;
    std::optional<std::string> resultId;
    std::vector<Diagnostic> items;
    DocumentUri uri;
    std::optional<std::int32_t> version;
};

using Definition = std::variant<Location, std::vector<Location>>;

using Declaration = std::variant<Location, std::vector<Location>>;

struct LspLiteral20 {
    std::vector<SymbolTag> valueSet;
};

struct DocumentSymbolClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral19> symbolKind;
    std::optional<bool> hierarchicalDocumentSymbolSupport;
    std::optional<LspLiteral20> tagSupport;
    std::optional<bool> labelSupport;
};

struct DidChangeConfigurationParams {
    LSPAny settings;
};

struct LogTraceParams {
    std::string message;
    std::optional<std::string> verbose;
};

struct LspLiteral45 {
    std::optional<std::string> language;
    std::optional<std::string> scheme;
    std::string pattern;
};

using TextDocumentFilter = std::variant<LspLiteral43, LspLiteral44, LspLiteral45>;

struct LspLiteral46 {
    std::string notebookType;
    std::optional<std::string> scheme;
    std::optional<std::string> pattern;
};

using NotebookDocumentFilter = std::variant<LspLiteral46, LspLiteral47, LspLiteral48>;

struct NotebookCellTextDocumentFilter {
    std::variant<std::string, NotebookDocumentFilter> notebook;
    std::optional<std::string> language;
};

using DocumentFilter = std::variant<TextDocumentFilter, NotebookCellTextDocumentFilter>;

using DocumentSelector = std::vector<DocumentFilter>;

struct TextDocumentRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
};

struct DocumentOnTypeFormattingRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::string firstTriggerCharacter;
    std::optional<std::vector<std::string>> moreTriggerCharacter;
};

struct TextDocumentSaveRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> includeText;
};

struct TextDocumentChangeRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    TextDocumentSyncKind syncKind;
};

struct LspLiteral14 {
    std::variant<std::string, NotebookDocumentFilter> notebook;
    std::optional<std::vector<LspLiteral15>> cells;
};

struct LspLiteral16 {
    std::optional<std::variant<std::string, NotebookDocumentFilter>> notebook;
    std::vector<LspLiteral15> cells;
};

struct NotebookDocumentSyncOptions {
    std::vector<std::variant<LspLiteral14, LspLiteral16>> notebookSelector;
    std::optional<bool> save;
};

struct UnchangedDocumentDiagnosticReport {
    rfl::Literal<"unchanged"> kind;
    std::string resultId;
};

struct WorkspaceUnchangedDocumentDiagnosticReport {
    rfl::Literal<"unchanged"> kind;
    std::string resultId;
    DocumentUri uri;
    std::optional<std::int32_t> version;
};

using WorkspaceDocumentDiagnosticReport =
    std::variant<WorkspaceFullDocumentDiagnosticReport, WorkspaceUnchangedDocumentDiagnosticReport>;

struct WorkspaceDiagnosticReportPartialResult {
    std::vector<WorkspaceDocumentDiagnosticReport> items;
};

struct WorkspaceDiagnosticReport {
    std::vector<WorkspaceDocumentDiagnosticReport> items;
};

struct RelatedUnchangedDocumentDiagnosticReport {
    rfl::Literal<"unchanged"> kind;
    std::string resultId;
    std::optional<
        std::map<DocumentUri,
                 std::variant<FullDocumentDiagnosticReport, UnchangedDocumentDiagnosticReport>>>
        relatedDocuments;
};

struct DocumentDiagnosticReportPartialResult {
    std::map<DocumentUri,
             std::variant<FullDocumentDiagnosticReport, UnchangedDocumentDiagnosticReport>>
        relatedDocuments;
};

struct RelatedFullDocumentDiagnosticReport {
    rfl::Literal<"full"> kind;
    std::optional<std::string> resultId;
    std::vector<Diagnostic> items;
    std::optional<
        std::map<DocumentUri,
                 std::variant<FullDocumentDiagnosticReport, UnchangedDocumentDiagnosticReport>>>
        relatedDocuments;
};

using DocumentDiagnosticReport =
    std::variant<RelatedFullDocumentDiagnosticReport, RelatedUnchangedDocumentDiagnosticReport>;

struct InitializedParams {};

struct TextDocumentIdentifier {
    DocumentUri uri;
};

struct VersionedTextDocumentIdentifier {
    DocumentUri uri;
    std::int32_t version;
};

struct TextDocumentPositionParams {
    TextDocumentIdentifier textDocument;
    Position position;
};

struct DidCloseNotebookDocumentParams {
    NotebookDocumentIdentifier notebookDocument;
    std::vector<TextDocumentIdentifier> cellTextDocuments;
};

struct DidCloseTextDocumentParams {
    TextDocumentIdentifier textDocument;
};

struct OptionalVersionedTextDocumentIdentifier {
    DocumentUri uri;
    std::optional<std::int32_t> version;
};

struct DocumentOnTypeFormattingParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::string ch;
    FormattingOptions options;
};

struct WillSaveTextDocumentParams {
    TextDocumentIdentifier textDocument;
    TextDocumentSaveReason reason;
};

struct DidSaveTextDocumentParams {
    TextDocumentIdentifier textDocument;
    std::optional<std::string> text;
};

struct SemanticTokensLegend {
    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;
};

struct LspLiteral21 {
    std::vector<std::string> properties;
};

struct WorkspaceSymbolClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral19> symbolKind;
    std::optional<LspLiteral20> tagSupport;
    std::optional<LspLiteral21> resolveSupport;
};

struct CompletionItemLabelDetails {
    std::optional<std::string> detail;
    std::optional<std::string> description;
};

struct StringValue {
    rfl::Literal<"snippet"> kind;
    std::string value;
};

struct InlineCompletionItem {
    std::variant<std::string, StringValue> insertText;
    std::optional<std::string> filterText;
    std::optional<Range> range;
    std::optional<Command> command;
};

struct InlineCompletionList {
    std::vector<InlineCompletionItem> items;
};

struct FileRename {
    std::string oldUri;
    std::string newUri;
};

struct RenameFilesParams {
    std::vector<FileRename> files;
};

struct DocumentHighlightClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct WorkDoneProgressReport {
    rfl::Literal<"report"> kind;
    std::optional<bool> cancellable;
    std::optional<std::string> message;
    std::optional<std::uint32_t> percentage;
};

struct TypeDefinitionClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
};

struct SemanticTokensEdit {
    std::uint32_t start;
    std::uint32_t deleteCount;
    std::optional<std::vector<std::uint32_t>> data;
};

struct SemanticTokensDeltaPartialResult {
    std::vector<SemanticTokensEdit> edits;
};

struct SemanticTokensDelta {
    std::optional<std::string> resultId;
    std::vector<SemanticTokensEdit> edits;
};

struct FileEvent {
    DocumentUri uri;
    FileChangeType type;
};

struct DidChangeWatchedFilesParams {
    std::vector<FileEvent> changes;
};

struct LspLiteral39 {
    bool defaultBehavior;
};

using PrepareRenameResult = std::variant<Range, LspLiteral38, LspLiteral39>;

struct LspLiteral5 {
    DocumentUri uri;
};

struct WorkDoneProgressOptions {
    std::optional<bool> workDoneProgress;
};

struct InlayHintOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct DocumentHighlightOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentHighlightRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct CodeLensOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct CodeLensRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct ExecuteCommandOptions {
    std::optional<bool> workDoneProgress;
    std::vector<std::string> commands;
};

struct ExecuteCommandRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::vector<std::string> commands;
};

struct RenameOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> prepareProvider;
};

struct RenameRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<bool> prepareProvider;
};

struct InlineCompletionOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentRangeFormattingOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> rangesSupport;
};

struct DocumentRangeFormattingRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<bool> rangesSupport;
};

struct DocumentColorOptions {
    std::optional<bool> workDoneProgress;
};

struct LinkedEditingRangeOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentFormattingOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentFormattingRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct FoldingRangeOptions {
    std::optional<bool> workDoneProgress;
};

struct MonikerOptions {
    std::optional<bool> workDoneProgress;
};

struct MonikerRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct InlineValueOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentSymbolOptions {
    std::optional<bool> workDoneProgress;
    std::optional<std::string> label;
};

struct DocumentSymbolRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> label;
};

struct DiagnosticOptions {
    std::optional<bool> workDoneProgress;
    std::optional<std::string> identifier;
    bool interFileDependencies;
    bool workspaceDiagnostics;
};

struct CompletionOptions {
    std::optional<bool> workDoneProgress;
    std::optional<std::vector<std::string>> triggerCharacters;
    std::optional<std::vector<std::string>> allCommitCharacters;
    std::optional<bool> resolveProvider;
    std::optional<LspLiteral13> completionItem;
};

struct CompletionRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::vector<std::string>> triggerCharacters;
    std::optional<std::vector<std::string>> allCommitCharacters;
    std::optional<bool> resolveProvider;
    std::optional<LspLiteral13> completionItem;
};

struct CallHierarchyOptions {
    std::optional<bool> workDoneProgress;
};

struct HoverOptions {
    std::optional<bool> workDoneProgress;
};

struct HoverRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct DefinitionOptions {
    std::optional<bool> workDoneProgress;
};

struct DefinitionRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct TypeHierarchyOptions {
    std::optional<bool> workDoneProgress;
};

struct SignatureHelpOptions {
    std::optional<bool> workDoneProgress;
    std::optional<std::vector<std::string>> triggerCharacters;
    std::optional<std::vector<std::string>> retriggerCharacters;
};

struct SignatureHelpRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::vector<std::string>> triggerCharacters;
    std::optional<std::vector<std::string>> retriggerCharacters;
};

struct ImplementationOptions {
    std::optional<bool> workDoneProgress;
};

struct ReferenceOptions {
    std::optional<bool> workDoneProgress;
};

struct ReferenceRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct DocumentLinkOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct DocumentLinkRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct DeclarationOptions {
    std::optional<bool> workDoneProgress;
};

struct WorkspaceSymbolOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct WorkspaceSymbolRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct TypeDefinitionOptions {
    std::optional<bool> workDoneProgress;
};

struct CodeActionOptions {
    std::optional<bool> workDoneProgress;
    std::optional<std::vector<CodeActionKind>> codeActionKinds;
    std::optional<bool> resolveProvider;
};

struct CodeActionRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::vector<CodeActionKind>> codeActionKinds;
    std::optional<bool> resolveProvider;
};

struct SelectionRangeOptions {
    std::optional<bool> workDoneProgress;
};

struct LspLiteral1 {
    std::string name;
    std::optional<std::string> version;
};

struct InlineValueClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct RenameFileOptions {
    std::optional<bool> overwrite;
    std::optional<bool> ignoreIfExists;
};

struct BaseSymbolInformation {
    std::string name;
    SymbolKind kind;
    std::optional<std::vector<SymbolTag>> tags;
    std::optional<std::string> containerName;
};

struct WorkspaceSymbol {
    std::string name;
    SymbolKind kind;
    std::optional<std::vector<SymbolTag>> tags;
    std::optional<std::string> containerName;
    std::variant<Location, LspLiteral5> location;
    std::optional<LSPAny> data;
};

struct SymbolInformation {
    std::string name;
    SymbolKind kind;
    std::optional<std::vector<SymbolTag>> tags;
    std::optional<std::string> containerName;
    std::optional<bool> deprecated;
    Location location;
};

struct HoverClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<std::vector<MarkupKind>> contentFormat;
};

struct InlayHintWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct LspLiteral33 {
    std::optional<bool> collapsedText;
};

struct ImplementationClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
};

struct LspLiteral41 {
    std::string text;
};

using TextDocumentContentChangeEvent = std::variant<LspLiteral40, LspLiteral41>;

struct LspLiteral10 {
    VersionedTextDocumentIdentifier document;
    std::vector<TextDocumentContentChangeEvent> changes;
};

struct DidChangeTextDocumentParams {
    VersionedTextDocumentIdentifier textDocument;
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};

struct DiagnosticClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> relatedDocumentSupport;
};

struct TextDocumentItem {
    DocumentUri uri;
    std::string languageId;
    std::int32_t version;
    std::string text;
};

struct LspLiteral9 {
    NotebookCellArrayChange array;
    std::optional<std::vector<TextDocumentItem>> didOpen;
    std::optional<std::vector<TextDocumentIdentifier>> didClose;
};

struct LspLiteral8 {
    std::optional<LspLiteral9> structure;
    std::optional<std::vector<NotebookCell>> data;
    std::optional<std::vector<LspLiteral10>> textContent;
};

struct NotebookDocumentChangeEvent {
    std::optional<LSPObject> metadata;
    std::optional<LspLiteral8> cells;
};

struct DidOpenNotebookDocumentParams {
    NotebookDocument notebookDocument;
    std::vector<TextDocumentItem> cellTextDocuments;
};

struct DidOpenTextDocumentParams {
    TextDocumentItem textDocument;
};

struct Unregistration {
    std::string id;
    std::string method;
};

struct UnregistrationParams {
    std::vector<Unregistration> unregisterations;
};

struct CancelParams {
    std::variant<std::int32_t, std::string> id;
};

struct LspLiteral25 {
    std::vector<InsertTextMode> valueSet;
};

struct LspLiteral22 {
    std::optional<bool> snippetSupport;
    std::optional<bool> commitCharactersSupport;
    std::optional<std::vector<MarkupKind>> documentationFormat;
    std::optional<bool> deprecatedSupport;
    std::optional<bool> preselectSupport;
    std::optional<LspLiteral23> tagSupport;
    std::optional<bool> insertReplaceSupport;
    std::optional<LspLiteral24> resolveSupport;
    std::optional<LspLiteral25> insertTextModeSupport;
    std::optional<bool> labelDetailsSupport;
};

struct CodeLensWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct VersionedNotebookDocumentIdentifier {
    std::int32_t version;
    URI uri;
};

struct DidChangeNotebookDocumentParams {
    VersionedNotebookDocumentIdentifier notebookDocument;
    NotebookDocumentChangeEvent change;
};

struct CodeLensClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral11 {
    std::string name;
    std::optional<std::string> version;
};

struct DefinitionClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
};

struct InlineCompletionClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral27 {
    std::optional<std::vector<std::string>> itemDefaults;
};

struct CompletionClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral22> completionItem;
    std::optional<LspLiteral26> completionItemKind;
    std::optional<InsertTextMode> insertTextMode;
    std::optional<bool> contextSupport;
    std::optional<LspLiteral27> completionList;
};

struct FoldingRange {
    std::uint32_t startLine;
    std::optional<std::uint32_t> startCharacter;
    std::uint32_t endLine;
    std::optional<std::uint32_t> endCharacter;
    std::optional<FoldingRangeKind> kind;
    std::optional<std::string> collapsedText;
};

struct StaticRegistrationOptions {
    std::optional<std::string> id;
};

struct InlineValueRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct DeclarationRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct CallHierarchyRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct InlineCompletionRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct ImplementationRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct SelectionRangeRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct LinkedEditingRangeRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct DiagnosticRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> identifier;
    bool interFileDependencies;
    bool workspaceDiagnostics;
    std::optional<std::string> id;
};

struct DocumentColorRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct FoldingRangeRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct InlayHintRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct TypeHierarchyRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct NotebookDocumentSyncRegistrationOptions {
    std::vector<std::variant<LspLiteral14, LspLiteral16>> notebookSelector;
    std::optional<bool> save;
    std::optional<std::string> id;
};

struct TypeDefinitionRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct ShowMessageParams {
    MessageType type;
    std::string message;
};

struct SetTraceParams {
    TraceValues value;
};

struct MessageActionItem {
    std::string title;
};

struct ShowMessageRequestParams {
    MessageType type;
    std::string message;
    std::optional<std::vector<MessageActionItem>> actions;
};

struct DidChangeConfigurationRegistrationOptions {
    std::optional<std::variant<std::string, std::vector<std::string>>> section;
};

struct ShowDocumentResult {
    bool success;
};

struct LspLiteral6 {};

struct LspLiteral35 {
    std::optional<std::variant<bool, LspLiteral6>> range;
    std::optional<std::variant<bool, LspLiteral36>> full;
};

struct SemanticTokensClientCapabilities {
    std::optional<bool> dynamicRegistration;
    LspLiteral35 requests;
    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;
    std::vector<TokenFormat> formats;
    std::optional<bool> overlappingTokenSupport;
    std::optional<bool> multilineTokenSupport;
    std::optional<bool> serverCancelSupport;
    std::optional<bool> augmentsSyntaxTokens;
};

struct SemanticTokensOptions {
    std::optional<bool> workDoneProgress;
    SemanticTokensLegend legend;
    std::optional<std::variant<bool, LspLiteral6>> range;
    std::optional<std::variant<bool, LspLiteral7>> full;
};

struct SemanticTokensRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    SemanticTokensLegend legend;
    std::optional<std::variant<bool, LspLiteral6>> range;
    std::optional<std::variant<bool, LspLiteral7>> full;
    std::optional<std::string> id;
};

struct ServerCapabilities {
    std::optional<PositionEncodingKind> positionEncoding;
    std::optional<std::variant<TextDocumentSyncOptions, TextDocumentSyncKind>> textDocumentSync;
    std::optional<
        std::variant<NotebookDocumentSyncOptions, NotebookDocumentSyncRegistrationOptions>>
        notebookDocumentSync;
    std::optional<CompletionOptions> completionProvider;
    std::optional<std::variant<bool, HoverOptions>> hoverProvider;
    std::optional<SignatureHelpOptions> signatureHelpProvider;
    std::optional<std::variant<bool, DeclarationOptions, DeclarationRegistrationOptions>>
        declarationProvider;
    std::optional<std::variant<bool, DefinitionOptions>> definitionProvider;
    std::optional<std::variant<bool, TypeDefinitionOptions, TypeDefinitionRegistrationOptions>>
        typeDefinitionProvider;
    std::optional<std::variant<bool, ImplementationOptions, ImplementationRegistrationOptions>>
        implementationProvider;
    std::optional<std::variant<bool, ReferenceOptions>> referencesProvider;
    std::optional<std::variant<bool, DocumentHighlightOptions>> documentHighlightProvider;
    std::optional<std::variant<bool, DocumentSymbolOptions>> documentSymbolProvider;
    std::optional<std::variant<bool, CodeActionOptions>> codeActionProvider;
    std::optional<CodeLensOptions> codeLensProvider;
    std::optional<DocumentLinkOptions> documentLinkProvider;
    std::optional<std::variant<bool, DocumentColorOptions, DocumentColorRegistrationOptions>>
        colorProvider;
    std::optional<std::variant<bool, WorkspaceSymbolOptions>> workspaceSymbolProvider;
    std::optional<std::variant<bool, DocumentFormattingOptions>> documentFormattingProvider;
    std::optional<std::variant<bool, DocumentRangeFormattingOptions>>
        documentRangeFormattingProvider;
    std::optional<DocumentOnTypeFormattingOptions> documentOnTypeFormattingProvider;
    std::optional<std::variant<bool, RenameOptions>> renameProvider;
    std::optional<std::variant<bool, FoldingRangeOptions, FoldingRangeRegistrationOptions>>
        foldingRangeProvider;
    std::optional<std::variant<bool, SelectionRangeOptions, SelectionRangeRegistrationOptions>>
        selectionRangeProvider;
    std::optional<ExecuteCommandOptions> executeCommandProvider;
    std::optional<std::variant<bool, CallHierarchyOptions, CallHierarchyRegistrationOptions>>
        callHierarchyProvider;
    std::optional<
        std::variant<bool, LinkedEditingRangeOptions, LinkedEditingRangeRegistrationOptions>>
        linkedEditingRangeProvider;
    std::optional<std::variant<SemanticTokensOptions, SemanticTokensRegistrationOptions>>
        semanticTokensProvider;
    std::optional<std::variant<bool, MonikerOptions, MonikerRegistrationOptions>> monikerProvider;
    std::optional<std::variant<bool, TypeHierarchyOptions, TypeHierarchyRegistrationOptions>>
        typeHierarchyProvider;
    std::optional<std::variant<bool, InlineValueOptions, InlineValueRegistrationOptions>>
        inlineValueProvider;
    std::optional<std::variant<bool, InlayHintOptions, InlayHintRegistrationOptions>>
        inlayHintProvider;
    std::optional<std::variant<DiagnosticOptions, DiagnosticRegistrationOptions>>
        diagnosticProvider;
    std::optional<std::variant<bool, InlineCompletionOptions>> inlineCompletionProvider;
    std::optional<LspLiteral12> workspace;
    std::optional<LSPAny> experimental;
};

struct InitializeResult {
    ServerCapabilities capabilities;
    std::optional<LspLiteral1> serverInfo;
};

struct MarkupContent {
    MarkupKind kind;
    std::string value;
};

struct CompletionItem {
    std::string label;
    std::optional<CompletionItemLabelDetails> labelDetails;
    std::optional<CompletionItemKind> kind;
    std::optional<std::vector<CompletionItemTag>> tags;
    std::optional<std::string> detail;
    std::optional<std::variant<std::string, MarkupContent>> documentation;
    std::optional<bool> deprecated;
    std::optional<bool> preselect;
    std::optional<std::string> sortText;
    std::optional<std::string> filterText;
    std::optional<std::string> insertText;
    std::optional<InsertTextFormat> insertTextFormat;
    std::optional<InsertTextMode> insertTextMode;
    std::optional<std::variant<TextEdit, InsertReplaceEdit>> textEdit;
    std::optional<std::string> textEditText;
    std::optional<std::vector<TextEdit>> additionalTextEdits;
    std::optional<std::vector<std::string>> commitCharacters;
    std::optional<Command> command;
    std::optional<LSPAny> data;
};

struct CompletionList {
    bool isIncomplete;
    std::optional<LspLiteral2> itemDefaults;
    std::vector<CompletionItem> items;
};

struct Hover {
    std::variant<MarkupContent, MarkedString, std::vector<MarkedString>> contents;
    std::optional<Range> range;
};

struct InlayHintLabelPart {
    std::string value;
    std::optional<std::variant<std::string, MarkupContent>> tooltip;
    std::optional<Location> location;
    std::optional<Command> command;
};

struct InlayHint {
    Position position;
    std::variant<std::string, std::vector<InlayHintLabelPart>> label;
    std::optional<InlayHintKind> kind;
    std::optional<std::vector<TextEdit>> textEdits;
    std::optional<std::variant<std::string, MarkupContent>> tooltip;
    std::optional<bool> paddingLeft;
    std::optional<bool> paddingRight;
    std::optional<LSPAny> data;
};

struct ParameterInformation {
    std::variant<std::string, std::tuple<std::uint32_t, std::uint32_t>> label;
    std::optional<std::variant<std::string, MarkupContent>> documentation;
};

struct SignatureInformation {
    std::string label;
    std::optional<std::variant<std::string, MarkupContent>> documentation;
    std::optional<std::vector<ParameterInformation>> parameters;
    std::optional<std::uint32_t> activeParameter;
};

struct SignatureHelp {
    std::vector<SignatureInformation> signatures;
    std::optional<std::uint32_t> activeSignature;
    std::optional<std::uint32_t> activeParameter;
};

struct SignatureHelpContext {
    SignatureHelpTriggerKind triggerKind;
    std::optional<std::string> triggerCharacter;
    bool isRetrigger;
    std::optional<SignatureHelp> activeSignatureHelp;
};

struct LinkedEditingRangeClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct CallHierarchyClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral34 {
    std::vector<DiagnosticTag> valueSet;
};

struct PublishDiagnosticsClientCapabilities {
    std::optional<bool> relatedInformation;
    std::optional<LspLiteral34> tagSupport;
    std::optional<bool> versionSupport;
    std::optional<bool> codeDescriptionSupport;
    std::optional<bool> dataSupport;
};

struct TextDocumentSyncClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> willSave;
    std::optional<bool> willSaveWaitUntil;
    std::optional<bool> didSave;
};

struct DocumentColorClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

using ChangeAnnotationIdentifier = std::string;

struct AnnotatedTextEdit {
    Range range;
    std::string newText;
    ChangeAnnotationIdentifier annotationId;
};

struct TextDocumentEdit {
    OptionalVersionedTextDocumentIdentifier textDocument;
    std::vector<std::variant<TextEdit, AnnotatedTextEdit>> edits;
};

struct ResourceOperation {
    std::string kind;
    std::optional<ChangeAnnotationIdentifier> annotationId;
};

struct CreateFile {
    std::optional<ChangeAnnotationIdentifier> annotationId;
    rfl::Literal<"create"> kind;
    DocumentUri uri;
    std::optional<CreateFileOptions> options;
};

struct RenameFile {
    std::optional<ChangeAnnotationIdentifier> annotationId;
    rfl::Literal<"rename"> kind;
    DocumentUri oldUri;
    DocumentUri newUri;
    std::optional<RenameFileOptions> options;
};

struct DeleteFile {
    std::optional<ChangeAnnotationIdentifier> annotationId;
    rfl::Literal<"delete"> kind;
    DocumentUri uri;
    std::optional<DeleteFileOptions> options;
};

struct WorkspaceEdit {
    std::optional<std::map<DocumentUri, std::vector<TextEdit>>> changes;
    std::optional<std::vector<std::variant<TextDocumentEdit, CreateFile, RenameFile, DeleteFile>>>
        documentChanges;
    std::optional<std::map<ChangeAnnotationIdentifier, ChangeAnnotation>> changeAnnotations;
};

struct ApplyWorkspaceEditParams {
    std::optional<std::string> label;
    WorkspaceEdit edit;
};

struct CodeAction {
    std::string title;
    std::optional<CodeActionKind> kind;
    std::optional<std::vector<Diagnostic>> diagnostics;
    std::optional<bool> isPreferred;
    std::optional<LspLiteral4> disabled;
    std::optional<WorkspaceEdit> edit;
    std::optional<Command> command;
    std::optional<LSPAny> data;
};

struct Color {
    double red;
    double green;
    double blue;
    double alpha;
};

struct ColorInformation {
    Range range;
    Color color;
};

struct DocumentLinkClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> tooltipSupport;
};

struct RegularExpressionsClientCapabilities {
    std::string engine;
    std::optional<std::string> version;
};

struct GeneralClientCapabilities {
    std::optional<LspLiteral17> staleRequestSupport;
    std::optional<RegularExpressionsClientCapabilities> regularExpressions;
    std::optional<MarkdownClientCapabilities> markdown;
    std::optional<std::vector<PositionEncodingKind>> positionEncodings;
};

struct FoldingRangeWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct WorkspaceClientCapabilities {
    std::optional<bool> applyEdit;
    std::optional<WorkspaceEditClientCapabilities> workspaceEdit;
    std::optional<DidChangeConfigurationClientCapabilities> didChangeConfiguration;
    std::optional<DidChangeWatchedFilesClientCapabilities> didChangeWatchedFiles;
    std::optional<WorkspaceSymbolClientCapabilities> symbol;
    std::optional<ExecuteCommandClientCapabilities> executeCommand;
    std::optional<bool> workspaceFolders;
    std::optional<bool> configuration;
    std::optional<SemanticTokensWorkspaceClientCapabilities> semanticTokens;
    std::optional<CodeLensWorkspaceClientCapabilities> codeLens;
    std::optional<FileOperationClientCapabilities> fileOperations;
    std::optional<InlineValueWorkspaceClientCapabilities> inlineValue;
    std::optional<InlayHintWorkspaceClientCapabilities> inlayHint;
    std::optional<DiagnosticWorkspaceClientCapabilities> diagnostics;
    std::optional<FoldingRangeWorkspaceClientCapabilities> foldingRange;
};

using ProgressToken = std::variant<std::int32_t, std::string>;

struct WorkDoneProgressCancelParams {
    ProgressToken token;
};

struct WorkDoneProgressCreateParams {
    ProgressToken token;
};

struct WorkDoneProgressParams {
    std::optional<ProgressToken> workDoneToken;
};

struct DocumentFormattingParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    FormattingOptions options;
};

struct SignatureHelpParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<SignatureHelpContext> context;
};

struct CallHierarchyPrepareParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct InlineValueParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Range range;
    InlineValueContext context;
};

struct DocumentRangesFormattingParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    std::vector<Range> ranges;
    FormattingOptions options;
};

struct RenameParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Position position;
    std::string newName;
};

struct LinkedEditingRangeParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct PrepareRenameParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct HoverParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct DocumentRangeFormattingParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Range range;
    FormattingOptions options;
};

struct ExecuteCommandParams {
    std::optional<ProgressToken> workDoneToken;
    std::string command;
    std::optional<std::vector<LSPAny>> arguments;
};

struct InlineCompletionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    InlineCompletionContext context;
};

struct InlayHintParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Range range;
};

struct TypeHierarchyPrepareParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct ProgressParams {
    ProgressToken token;
    LSPAny value;
};

struct PartialResultParams {
    std::optional<ProgressToken> partialResultToken;
};

struct SemanticTokensDeltaParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    std::string previousResultId;
};

struct SelectionRangeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    std::vector<Position> positions;
};

struct TypeDefinitionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct CodeLensParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct TypeHierarchySubtypesParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TypeHierarchyItem item;
};

struct WorkspaceSymbolParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    std::string query;
};

struct TypeHierarchySupertypesParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TypeHierarchyItem item;
};

struct DeclarationParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct DocumentSymbolParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct SemanticTokensRangeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    Range range;
};

struct CodeActionParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    Range range;
    CodeActionContext context;
};

struct ImplementationParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct SemanticTokensParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct CallHierarchyIncomingCallsParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    CallHierarchyItem item;
};

struct WorkspaceDiagnosticParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    std::optional<std::string> identifier;
    std::vector<PreviousResultId> previousResultIds;
};

struct DefinitionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct FoldingRangeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct DocumentLinkParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct DocumentColorParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct ColorPresentationParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    Color color;
    Range range;
};

struct MonikerParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct ReferenceParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    ReferenceContext context;
};

struct CallHierarchyOutgoingCallsParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    CallHierarchyItem item;
};

struct DocumentDiagnosticParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    std::optional<std::string> identifier;
    std::optional<std::string> previousResultId;
};

struct DocumentHighlightParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct LspLiteral32 {
    std::optional<std::vector<FoldingRangeKind>> valueSet;
};

struct FoldingRangeClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<std::uint32_t> rangeLimit;
    std::optional<bool> lineFoldingOnly;
    std::optional<LspLiteral32> foldingRangeKind;
    std::optional<LspLiteral33> foldingRange;
};

struct FileCreate {
    std::string uri;
};

struct CreateFilesParams {
    std::vector<FileCreate> files;
};

struct DocumentOnTypeFormattingClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct CompletionContext {
    CompletionTriggerKind triggerKind;
    std::optional<std::string> triggerCharacter;
};

struct CompletionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    std::optional<CompletionContext> context;
};

struct DeclarationClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
};

struct TextDocumentClientCapabilities {
    std::optional<TextDocumentSyncClientCapabilities> synchronization;
    std::optional<CompletionClientCapabilities> completion;
    std::optional<HoverClientCapabilities> hover;
    std::optional<SignatureHelpClientCapabilities> signatureHelp;
    std::optional<DeclarationClientCapabilities> declaration;
    std::optional<DefinitionClientCapabilities> definition;
    std::optional<TypeDefinitionClientCapabilities> typeDefinition;
    std::optional<ImplementationClientCapabilities> implementation;
    std::optional<ReferenceClientCapabilities> references;
    std::optional<DocumentHighlightClientCapabilities> documentHighlight;
    std::optional<DocumentSymbolClientCapabilities> documentSymbol;
    std::optional<CodeActionClientCapabilities> codeAction;
    std::optional<CodeLensClientCapabilities> codeLens;
    std::optional<DocumentLinkClientCapabilities> documentLink;
    std::optional<DocumentColorClientCapabilities> colorProvider;
    std::optional<DocumentFormattingClientCapabilities> formatting;
    std::optional<DocumentRangeFormattingClientCapabilities> rangeFormatting;
    std::optional<DocumentOnTypeFormattingClientCapabilities> onTypeFormatting;
    std::optional<RenameClientCapabilities> rename;
    std::optional<FoldingRangeClientCapabilities> foldingRange;
    std::optional<SelectionRangeClientCapabilities> selectionRange;
    std::optional<PublishDiagnosticsClientCapabilities> publishDiagnostics;
    std::optional<CallHierarchyClientCapabilities> callHierarchy;
    std::optional<SemanticTokensClientCapabilities> semanticTokens;
    std::optional<LinkedEditingRangeClientCapabilities> linkedEditingRange;
    std::optional<MonikerClientCapabilities> moniker;
    std::optional<TypeHierarchyClientCapabilities> typeHierarchy;
    std::optional<InlineValueClientCapabilities> inlineValue;
    std::optional<InlayHintClientCapabilities> inlayHint;
    std::optional<DiagnosticClientCapabilities> diagnostic;
    std::optional<InlineCompletionClientCapabilities> inlineCompletion;
};

struct ClientCapabilities {
    std::optional<WorkspaceClientCapabilities> workspace;
    std::optional<TextDocumentClientCapabilities> textDocument;
    std::optional<NotebookDocumentClientCapabilities> notebookDocument;
    std::optional<WindowClientCapabilities> window;
    std::optional<GeneralClientCapabilities> general;
    std::optional<LSPAny> experimental;
};

struct _InitializeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<std::int32_t> processId;
    std::optional<LspLiteral11> clientInfo;
    std::optional<std::string> locale;
    std::optional<std::string> rootPath;
    std::optional<DocumentUri> rootUri;
    ClientCapabilities capabilities;
    std::optional<LSPAny> initializationOptions;
    std::optional<TraceValues> trace;
};

struct InitializeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<std::int32_t> processId;
    std::optional<LspLiteral11> clientInfo;
    std::optional<std::string> locale;
    std::optional<std::string> rootPath;
    std::optional<DocumentUri> rootUri;
    ClientCapabilities capabilities;
    std::optional<LSPAny> initializationOptions;
    std::optional<TraceValues> trace;
    std::optional<std::vector<WorkspaceFolder>> workspaceFolders;
};

struct Moniker {
    std::string scheme;
    std::string identifier;
    UniquenessLevel unique;
    std::optional<MonikerKind> kind;
};

}  // namespace eventide::language::proto

namespace refl::serde {

template <>
struct enum_traits<eventide::language::proto::SemanticTokenTypesEnum> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::SemanticTokenTypesEnum, std::string_view>, 23>
            mapping = {
                {
                 {eventide::language::proto::SemanticTokenTypesEnum::namespace_, "namespace"},
                 {eventide::language::proto::SemanticTokenTypesEnum::type, "type"},
                 {eventide::language::proto::SemanticTokenTypesEnum::class_, "class"},
                 {eventide::language::proto::SemanticTokenTypesEnum::enum_, "enum"},
                 {eventide::language::proto::SemanticTokenTypesEnum::interface, "interface"},
                 {eventide::language::proto::SemanticTokenTypesEnum::struct_, "struct"},
                 {eventide::language::proto::SemanticTokenTypesEnum::typeParameter,
                     "typeParameter"},
                 {eventide::language::proto::SemanticTokenTypesEnum::parameter, "parameter"},
                 {eventide::language::proto::SemanticTokenTypesEnum::variable, "variable"},
                 {eventide::language::proto::SemanticTokenTypesEnum::property, "property"},
                 {eventide::language::proto::SemanticTokenTypesEnum::enumMember, "enumMember"},
                 {eventide::language::proto::SemanticTokenTypesEnum::event, "event"},
                 {eventide::language::proto::SemanticTokenTypesEnum::function, "function"},
                 {eventide::language::proto::SemanticTokenTypesEnum::method, "method"},
                 {eventide::language::proto::SemanticTokenTypesEnum::macro, "macro"},
                 {eventide::language::proto::SemanticTokenTypesEnum::keyword, "keyword"},
                 {eventide::language::proto::SemanticTokenTypesEnum::modifier, "modifier"},
                 {eventide::language::proto::SemanticTokenTypesEnum::comment, "comment"},
                 {eventide::language::proto::SemanticTokenTypesEnum::string, "string"},
                 {eventide::language::proto::SemanticTokenTypesEnum::number, "number"},
                 {eventide::language::proto::SemanticTokenTypesEnum::regexp, "regexp"},
                 {eventide::language::proto::SemanticTokenTypesEnum::operator_, "operator"},
                 {eventide::language::proto::SemanticTokenTypesEnum::decorator, "decorator"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::SemanticTokenModifiersEnum> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::array<
        std::pair<eventide::language::proto::SemanticTokenModifiersEnum, std::string_view>,
        10>
        mapping = {
            {
             {eventide::language::proto::SemanticTokenModifiersEnum::declaration, "declaration"},
             {eventide::language::proto::SemanticTokenModifiersEnum::definition, "definition"},
             {eventide::language::proto::SemanticTokenModifiersEnum::readonly, "readonly"},
             {eventide::language::proto::SemanticTokenModifiersEnum::static_, "static"},
             {eventide::language::proto::SemanticTokenModifiersEnum::deprecated, "deprecated"},
             {eventide::language::proto::SemanticTokenModifiersEnum::abstract, "abstract"},
             {eventide::language::proto::SemanticTokenModifiersEnum::async, "async"},
             {eventide::language::proto::SemanticTokenModifiersEnum::modification,
                 "modification"},
             {eventide::language::proto::SemanticTokenModifiersEnum::documentation,
                 "documentation"},
             {eventide::language::proto::SemanticTokenModifiersEnum::defaultLibrary,
                 "defaultLibrary"},
             }
    };
};

template <>
struct enum_traits<eventide::language::proto::DocumentDiagnosticReportKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::array<
        std::pair<eventide::language::proto::DocumentDiagnosticReportKind, std::string_view>,
        2>
        mapping = {
            {
             {eventide::language::proto::DocumentDiagnosticReportKind::Full, "full"},
             {eventide::language::proto::DocumentDiagnosticReportKind::Unchanged, "unchanged"},
             }
    };
};

template <>
struct enum_traits<eventide::language::proto::ErrorCodes> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::LSPErrorCodes> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::FoldingRangeKindEnum> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::FoldingRangeKindEnum, std::string_view>, 3>
            mapping = {
                {
                 {eventide::language::proto::FoldingRangeKindEnum::Comment, "comment"},
                 {eventide::language::proto::FoldingRangeKindEnum::Imports, "imports"},
                 {eventide::language::proto::FoldingRangeKindEnum::Region, "region"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::SymbolKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::SymbolTag> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::UniquenessLevel> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::UniquenessLevel, std::string_view>, 5>
            mapping = {
                {
                 {eventide::language::proto::UniquenessLevel::document, "document"},
                 {eventide::language::proto::UniquenessLevel::project, "project"},
                 {eventide::language::proto::UniquenessLevel::group, "group"},
                 {eventide::language::proto::UniquenessLevel::scheme, "scheme"},
                 {eventide::language::proto::UniquenessLevel::global, "global"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::MonikerKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::array<std::pair<eventide::language::proto::MonikerKind, std::string_view>,
                                3>
        mapping = {
            {
             {eventide::language::proto::MonikerKind::import, "import"},
             {eventide::language::proto::MonikerKind::export_, "export"},
             {eventide::language::proto::MonikerKind::local, "local"},
             }
    };
};

template <>
struct enum_traits<eventide::language::proto::InlayHintKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::MessageType> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::TextDocumentSyncKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::TextDocumentSaveReason> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::CompletionItemKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::CompletionItemTag> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::InsertTextFormat> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::InsertTextMode> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::DocumentHighlightKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::CodeActionKindEnum> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::CodeActionKindEnum, std::string_view>, 9>
            mapping = {
                {
                 {eventide::language::proto::CodeActionKindEnum::Empty, ""},
                 {eventide::language::proto::CodeActionKindEnum::QuickFix, "quickfix"},
                 {eventide::language::proto::CodeActionKindEnum::Refactor, "refactor"},
                 {eventide::language::proto::CodeActionKindEnum::RefactorExtract,
                     "refactor.extract"},
                 {eventide::language::proto::CodeActionKindEnum::RefactorInline,
                     "refactor.inline"},
                 {eventide::language::proto::CodeActionKindEnum::RefactorRewrite,
                     "refactor.rewrite"},
                 {eventide::language::proto::CodeActionKindEnum::Source, "source"},
                 {eventide::language::proto::CodeActionKindEnum::SourceOrganizeImports,
                     "source.organizeImports"},
                 {eventide::language::proto::CodeActionKindEnum::SourceFixAll, "source.fixAll"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::TraceValues> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::array<std::pair<eventide::language::proto::TraceValues, std::string_view>,
                                3>
        mapping = {
            {
             {eventide::language::proto::TraceValues::Off, "off"},
             {eventide::language::proto::TraceValues::Messages, "messages"},
             {eventide::language::proto::TraceValues::Verbose, "verbose"},
             }
    };
};

template <>
struct enum_traits<eventide::language::proto::MarkupKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::array<std::pair<eventide::language::proto::MarkupKind, std::string_view>,
                                2>
        mapping = {
            {
             {eventide::language::proto::MarkupKind::PlainText, "plaintext"},
             {eventide::language::proto::MarkupKind::Markdown, "markdown"},
             }
    };
};

template <>
struct enum_traits<eventide::language::proto::InlineCompletionTriggerKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::PositionEncodingKindEnum> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::PositionEncodingKindEnum, std::string_view>, 3>
            mapping = {
                {
                 {eventide::language::proto::PositionEncodingKindEnum::UTF8, "utf-8"},
                 {eventide::language::proto::PositionEncodingKindEnum::UTF16, "utf-16"},
                 {eventide::language::proto::PositionEncodingKindEnum::UTF32, "utf-32"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::FileChangeType> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::WatchKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::DiagnosticSeverity> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::DiagnosticTag> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::CompletionTriggerKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::SignatureHelpTriggerKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::CodeActionTriggerKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::FileOperationPatternKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::FileOperationPatternKind, std::string_view>, 2>
            mapping = {
                {
                 {eventide::language::proto::FileOperationPatternKind::file, "file"},
                 {eventide::language::proto::FileOperationPatternKind::folder, "folder"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::NotebookCellKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::ResourceOperationKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::ResourceOperationKind, std::string_view>, 3>
            mapping = {
                {
                 {eventide::language::proto::ResourceOperationKind::Create, "create"},
                 {eventide::language::proto::ResourceOperationKind::Rename, "rename"},
                 {eventide::language::proto::ResourceOperationKind::Delete, "delete"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::FailureHandlingKind> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::
        array<std::pair<eventide::language::proto::FailureHandlingKind, std::string_view>, 4>
            mapping = {
                {
                 {eventide::language::proto::FailureHandlingKind::Abort, "abort"},
                 {eventide::language::proto::FailureHandlingKind::Transactional,
                     "transactional"},
                 {eventide::language::proto::FailureHandlingKind::TextOnlyTransactional,
                     "textOnlyTransactional"},
                 {eventide::language::proto::FailureHandlingKind::Undo, "undo"},
                 }
    };
};

template <>
struct enum_traits<eventide::language::proto::PrepareSupportDefaultBehavior> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::integer;
};

template <>
struct enum_traits<eventide::language::proto::TokenFormat> {
    constexpr static bool enabled = true;
    constexpr static enum_encoding encoding = enum_encoding::string;
    constexpr static std::array<std::pair<eventide::language::proto::TokenFormat, std::string_view>,
                                1>
        mapping = {
            {
             {eventide::language::proto::TokenFormat::Relative, "relative"},
             }
    };
};

}  // namespace refl::serde
