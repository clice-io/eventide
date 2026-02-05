#pragma once

#include "language/lsp_base.h"

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

struct LspLiteral11 {
    std::string name;
    std::optional<std::string> version;
};

struct DocumentColorClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral33 {
    std::optional<bool> collapsedText;
};

struct Registration {
    std::string id;
    std::string method;
    std::optional<LSPAny> registerOptions;
};

struct RegistrationParams {
    std::vector<Registration> registrations;
};

struct LspLiteral21 {
    std::vector<std::string> properties;
};

struct ShowDocumentClientCapabilities {
    bool support;
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

struct DeclarationClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
};

using Pattern = std::string;

struct Moniker {
    std::string scheme;
    std::string identifier;
    UniquenessLevel unique;
    std::optional<MonikerKind> kind;
};

struct FileCreate {
    std::string uri;
};

struct CreateFilesParams {
    std::vector<FileCreate> files;
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

struct LspLiteral37 {
    std::optional<bool> additionalPropertiesSupport;
};

struct ShowMessageRequestClientCapabilities {
    std::optional<LspLiteral37> messageActionItem;
};

struct WindowClientCapabilities {
    std::optional<bool> workDoneProgress;
    std::optional<ShowMessageRequestClientCapabilities> showMessage;
    std::optional<ShowDocumentClientCapabilities> showDocument;
};

struct LinkedEditingRangeClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral48 {
    std::optional<std::string> notebookType;
    std::optional<std::string> scheme;
    std::string pattern;
};

struct DocumentRangeFormattingClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> rangesSupport;
};

struct SelectionRangeClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct CompletionItemLabelDetails {
    std::optional<std::string> detail;
    std::optional<std::string> description;
};

struct ReferenceContext {
    bool includeDeclaration;
};

struct WorkDoneProgressBegin {
    rfl::Literal<"begin"> kind;
    std::string title;
    std::optional<bool> cancellable;
    std::optional<std::string> message;
    std::optional<std::uint32_t> percentage;
};

struct LogMessageParams {
    MessageType type;
    std::string message;
};

struct FileEvent {
    DocumentUri uri;
    FileChangeType type;
};

struct DidChangeWatchedFilesParams {
    std::vector<FileEvent> changes;
};

struct LspLiteral15 {
    std::string language;
};

struct DidChangeConfigurationRegistrationOptions {
    std::optional<std::variant<std::string, std::vector<std::string>>> section;
};

struct NotebookDocumentIdentifier {
    URI uri;
};

struct DidSaveNotebookDocumentParams {
    NotebookDocumentIdentifier notebookDocument;
};

struct CodeLensClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct WorkspaceFolder {
    URI uri;
    std::string name;
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

struct LspLiteral44 {
    std::optional<std::string> language;
    std::string scheme;
    std::optional<std::string> pattern;
};

struct WorkDoneProgressReport {
    rfl::Literal<"report"> kind;
    std::optional<bool> cancellable;
    std::optional<std::string> message;
    std::optional<std::uint32_t> percentage;
};

struct LspLiteral13 {
    std::optional<bool> labelDetailsSupport;
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

struct ConfigurationItem {
    std::optional<URI> scopeUri;
    std::optional<std::string> section;
};

struct ConfigurationParams {
    std::vector<ConfigurationItem> items;
};

struct LspLiteral19 {
    std::optional<std::vector<SymbolKind>> valueSet;
};

struct CallHierarchyClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral1 {
    std::string name;
    std::optional<std::string> version;
};

struct ChangeAnnotation {
    std::string label;
    std::optional<bool> needsConfirmation;
    std::optional<std::string> description;
};

struct ShowMessageParams {
    MessageType type;
    std::string message;
};

struct SemanticTokensLegend {
    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;
};

struct RenameClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> prepareSupport;
    std::optional<PrepareSupportDefaultBehavior> prepareSupportDefaultBehavior;
    std::optional<bool> honorsChangeAnnotations;
};

struct DiagnosticServerCancellationData {
    bool retriggerRequest;
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

struct DidChangeWatchedFilesClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> relativePatternSupport;
};

struct DocumentOnTypeFormattingClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

using ChangeAnnotationIdentifier = std::string;

struct ResourceOperation {
    std::string kind;
    std::optional<ChangeAnnotationIdentifier> annotationId;
};

struct CodeDescription {
    URI href;
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

struct StaticRegistrationOptions {
    std::optional<std::string> id;
};

struct StringValue {
    rfl::Literal<"snippet"> kind;
    std::string value;
};

struct PreviousResultId {
    DocumentUri uri;
    std::string value;
};

struct Position {
    std::uint32_t line;
    std::uint32_t character;
};

struct Range {
    Position start;
    Position end;
};

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

struct InlineValueVariableLookup {
    Range range;
    std::optional<std::string> variableName;
    bool caseSensitiveLookup;
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

struct InlineValueEvaluatableExpression {
    Range range;
    std::optional<std::string> expression;
};

struct LspLiteral38 {
    Range range;
    std::string placeholder;
};

struct ShowDocumentParams {
    URI uri;
    std::optional<bool> external;
    std::optional<bool> takeFocus;
    std::optional<Range> selection;
};

struct DocumentLink {
    Range range;
    std::optional<URI> target;
    std::optional<std::string> tooltip;
    std::optional<LSPAny> data;
};

struct DocumentHighlight {
    Range range;
    std::optional<DocumentHighlightKind> kind;
};

struct LspLiteral40 {
    Range range;
    std::optional<std::uint32_t> rangeLength;
    std::string text;
};

struct SelectedCompletionInfo {
    Range range;
    std::string text;
};

struct InlineCompletionContext {
    InlineCompletionTriggerKind triggerKind;
    std::optional<SelectedCompletionInfo> selectedCompletionInfo;
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

struct LinkedEditingRanges {
    std::vector<Range> ranges;
    std::optional<std::string> wordPattern;
};

struct SelectionRange {
    Range range;
    std::optional<std::shared_ptr<SelectionRange>> parent;
};

struct LocationLink {
    std::optional<Range> originSelectionRange;
    DocumentUri targetUri;
    Range targetRange;
    Range targetSelectionRange;
};

using DefinitionLink = LocationLink;

using DeclarationLink = LocationLink;

struct InlineValueText {
    Range range;
    std::string text;
};

using InlineValue =
    std::variant<InlineValueText, InlineValueVariableLookup, InlineValueEvaluatableExpression>;

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

using Declaration = std::variant<Location, std::vector<Location>>;

using Definition = std::variant<Location, std::vector<Location>>;

struct InsertReplaceEdit {
    std::string newText;
    Range insert;
    Range replace;
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

struct AnnotatedTextEdit {
    Range range;
    std::string newText;
    ChangeAnnotationIdentifier annotationId;
};

struct InlineValueContext {
    std::int32_t frameId;
    Range stoppedLocation;
};

struct InitializeError {
    bool retry;
};

struct DocumentOnTypeFormattingOptions {
    std::string firstTriggerCharacter;
    std::optional<std::vector<std::string>> moreTriggerCharacter;
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

struct LspLiteral42 {
    std::string language;
    std::string value;
};

using MarkedString = std::variant<std::string, LspLiteral42>;

struct LspLiteral46 {
    std::string notebookType;
    std::optional<std::string> scheme;
    std::optional<std::string> pattern;
};

struct TypeHierarchyClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

using ProgressToken = std::variant<std::int32_t, std::string>;

struct WorkDoneProgressParams {
    std::optional<ProgressToken> workDoneToken;
};

struct ExecuteCommandParams {
    std::optional<ProgressToken> workDoneToken;
    std::string command;
    std::optional<std::vector<LSPAny>> arguments;
};

struct ProgressParams {
    ProgressToken token;
    LSPAny value;
};

struct WorkDoneProgressCancelParams {
    ProgressToken token;
};

struct PartialResultParams {
    std::optional<ProgressToken> partialResultToken;
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

struct CallHierarchyIncomingCallsParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    CallHierarchyItem item;
};

struct TypeHierarchySubtypesParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TypeHierarchyItem item;
};

struct CallHierarchyOutgoingCallsParams {
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

struct WorkDoneProgressCreateParams {
    ProgressToken token;
};

struct LspLiteral4 {
    std::string reason;
};

struct FoldingRange {
    std::uint32_t startLine;
    std::optional<std::uint32_t> startCharacter;
    std::uint32_t endLine;
    std::optional<std::uint32_t> endCharacter;
    std::optional<FoldingRangeKind> kind;
    std::optional<std::string> collapsedText;
};

struct DeleteFileOptions {
    std::optional<bool> recursive;
    std::optional<bool> ignoreIfNotExists;
};

struct DeleteFile {
    std::optional<ChangeAnnotationIdentifier> annotationId;
    rfl::Literal<"delete"> kind;
    DocumentUri uri;
    std::optional<DeleteFileOptions> options;
};

struct SemanticTokens {
    std::optional<std::string> resultId;
    std::vector<std::uint32_t> data;
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

struct MarkupContent {
    MarkupKind kind;
    std::string value;
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

struct Hover {
    std::variant<MarkupContent, MarkedString, std::vector<MarkedString>> contents;
    std::optional<Range> range;
};

struct ExecuteCommandClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral47 {
    std::optional<std::string> notebookType;
    std::string scheme;
    std::optional<std::string> pattern;
};

using NotebookDocumentFilter = std::variant<LspLiteral46, LspLiteral47, LspLiteral48>;

struct LspLiteral16 {
    std::optional<std::variant<std::string, NotebookDocumentFilter>> notebook;
    std::vector<LspLiteral15> cells;
};

struct LspLiteral14 {
    std::variant<std::string, NotebookDocumentFilter> notebook;
    std::optional<std::vector<LspLiteral15>> cells;
};

struct NotebookDocumentSyncOptions {
    std::vector<std::variant<LspLiteral14, LspLiteral16>> notebookSelector;
    std::optional<bool> save;
};

struct NotebookDocumentSyncRegistrationOptions {
    std::vector<std::variant<LspLiteral14, LspLiteral16>> notebookSelector;
    std::optional<bool> save;
    std::optional<std::string> id;
};

struct NotebookCellTextDocumentFilter {
    std::variant<std::string, NotebookDocumentFilter> notebook;
    std::optional<std::string> language;
};

struct ImplementationClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
};

struct DefinitionClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
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

struct MessageActionItem {
    std::string title;
};

struct ShowMessageRequestParams {
    MessageType type;
    std::string message;
    std::optional<std::vector<MessageActionItem>> actions;
};

struct InlayHintWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct DocumentHighlightClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct CompletionContext {
    CompletionTriggerKind triggerKind;
    std::optional<std::string> triggerCharacter;
};

struct WorkspaceFoldersServerCapabilities {
    std::optional<bool> supported;
    std::optional<std::variant<std::string, bool>> changeNotifications;
};

struct LspLiteral12 {
    std::optional<WorkspaceFoldersServerCapabilities> workspaceFolders;
    std::optional<FileOperationOptions> fileOperations;
};

struct Command {
    std::string title;
    std::string command;
    std::optional<std::vector<LSPAny>> arguments;
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

struct CodeLens {
    Range range;
    std::optional<Command> command;
    std::optional<LSPAny> data;
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

struct InlineValueClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral36 {
    std::optional<bool> delta;
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

struct LspLiteral25 {
    std::vector<InsertTextMode> valueSet;
};

struct DidChangeConfigurationClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral23 {
    std::vector<CompletionItemTag> valueSet;
};

struct LspLiteral27 {
    std::optional<std::vector<std::string>> itemDefaults;
};

struct ReferenceClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral26 {
    std::optional<std::vector<CompletionItemKind>> valueSet;
};

struct DiagnosticWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct LspLiteral39 {
    bool defaultBehavior;
};

using PrepareRenameResult = std::variant<Range, LspLiteral38, LspLiteral39>;

struct DocumentFormattingClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct TextDocumentSyncClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> willSave;
    std::optional<bool> willSaveWaitUntil;
    std::optional<bool> didSave;
};

struct CancelParams {
    std::variant<std::int32_t, std::string> id;
};

struct SemanticTokensPartialResult {
    std::vector<std::uint32_t> data;
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

struct LspLiteral45 {
    std::optional<std::string> language;
    std::optional<std::string> scheme;
    std::string pattern;
};

struct RenameFileOptions {
    std::optional<bool> overwrite;
    std::optional<bool> ignoreIfExists;
};

struct RenameFile {
    std::optional<ChangeAnnotationIdentifier> annotationId;
    rfl::Literal<"rename"> kind;
    DocumentUri oldUri;
    DocumentUri newUri;
    std::optional<RenameFileOptions> options;
};

struct SemanticTokensWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct FormattingOptions {
    std::uint32_t tabSize;
    bool insertSpaces;
    std::optional<bool> trimTrailingWhitespace;
    std::optional<bool> insertFinalNewline;
    std::optional<bool> trimFinalNewlines;
};

struct FoldingRangeWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct CodeLensWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct ShowDocumentResult {
    bool success;
};

struct LspLiteral5 {
    DocumentUri uri;
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

struct UnchangedDocumentDiagnosticReport {
    rfl::Literal<"unchanged"> kind;
    std::string resultId;
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

struct RelatedUnchangedDocumentDiagnosticReport {
    rfl::Literal<"unchanged"> kind;
    std::string resultId;
    std::optional<
        std::map<DocumentUri,
                 std::variant<FullDocumentDiagnosticReport, UnchangedDocumentDiagnosticReport>>>
        relatedDocuments;
};

using DocumentDiagnosticReport =
    std::variant<RelatedFullDocumentDiagnosticReport, RelatedUnchangedDocumentDiagnosticReport>;

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

struct DocumentDiagnosticReportPartialResult {
    std::map<DocumentUri,
             std::variant<FullDocumentDiagnosticReport, UnchangedDocumentDiagnosticReport>>
        relatedDocuments;
};

struct TypeDefinitionClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> linkSupport;
};

struct FileDelete {
    std::string uri;
};

struct DeleteFilesParams {
    std::vector<FileDelete> files;
};

struct SemanticTokensEdit {
    std::uint32_t start;
    std::uint32_t deleteCount;
    std::optional<std::vector<std::uint32_t>> data;
};

struct SemanticTokensDelta {
    std::optional<std::string> resultId;
    std::vector<SemanticTokensEdit> edits;
};

struct SemanticTokensDeltaPartialResult {
    std::vector<SemanticTokensEdit> edits;
};

struct CreateFileOptions {
    std::optional<bool> overwrite;
    std::optional<bool> ignoreIfExists;
};

struct CreateFile {
    std::optional<ChangeAnnotationIdentifier> annotationId;
    rfl::Literal<"create"> kind;
    DocumentUri uri;
    std::optional<CreateFileOptions> options;
};

struct TextDocumentItem {
    DocumentUri uri;
    std::string languageId;
    std::int32_t version;
    std::string text;
};

struct DidOpenNotebookDocumentParams {
    NotebookDocument notebookDocument;
    std::vector<TextDocumentItem> cellTextDocuments;
};

struct DidOpenTextDocumentParams {
    TextDocumentItem textDocument;
};

struct NotebookDocumentSyncClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> executionSummarySupport;
};

struct NotebookDocumentClientCapabilities {
    NotebookDocumentSyncClientCapabilities synchronization;
};

struct DocumentLinkClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> tooltipSupport;
};

struct LogTraceParams {
    std::string message;
    std::optional<std::string> verbose;
};

struct LspLiteral43 {
    std::string language;
    std::optional<std::string> scheme;
    std::optional<std::string> pattern;
};

using TextDocumentFilter = std::variant<LspLiteral43, LspLiteral44, LspLiteral45>;

using DocumentFilter = std::variant<TextDocumentFilter, NotebookCellTextDocumentFilter>;

using DocumentSelector = std::vector<DocumentFilter>;

struct TextDocumentRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
};

struct TextDocumentChangeRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    TextDocumentSyncKind syncKind;
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

struct WorkDoneProgressOptions {
    std::optional<bool> workDoneProgress;
};

struct ReferenceOptions {
    std::optional<bool> workDoneProgress;
};

struct ReferenceRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct TypeDefinitionOptions {
    std::optional<bool> workDoneProgress;
};

struct TypeDefinitionRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
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

struct CallHierarchyOptions {
    std::optional<bool> workDoneProgress;
};

struct CallHierarchyRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct WorkspaceSymbolOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct WorkspaceSymbolRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct DiagnosticOptions {
    std::optional<bool> workDoneProgress;
    std::optional<std::string> identifier;
    bool interFileDependencies;
    bool workspaceDiagnostics;
};

struct DiagnosticRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> identifier;
    bool interFileDependencies;
    bool workspaceDiagnostics;
    std::optional<std::string> id;
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

struct ImplementationOptions {
    std::optional<bool> workDoneProgress;
};

struct ImplementationRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct InlineCompletionOptions {
    std::optional<bool> workDoneProgress;
};

struct InlineCompletionRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
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

struct InlineValueOptions {
    std::optional<bool> workDoneProgress;
};

struct InlineValueRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct DocumentHighlightOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentHighlightRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
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

struct DocumentFormattingOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentFormattingRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
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

struct SelectionRangeOptions {
    std::optional<bool> workDoneProgress;
};

struct SelectionRangeRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct DeclarationOptions {
    std::optional<bool> workDoneProgress;
};

struct DeclarationRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
};

struct InlayHintOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
};

struct InlayHintRegistrationOptions {
    std::optional<bool> workDoneProgress;
    std::optional<bool> resolveProvider;
    std::optional<DocumentSelector> documentSelector;
    std::optional<std::string> id;
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

struct LinkedEditingRangeOptions {
    std::optional<bool> workDoneProgress;
};

struct LinkedEditingRangeRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct DocumentColorOptions {
    std::optional<bool> workDoneProgress;
};

struct DocumentColorRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
};

struct TypeHierarchyOptions {
    std::optional<bool> workDoneProgress;
};

struct TypeHierarchyRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
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

struct FoldingRangeOptions {
    std::optional<bool> workDoneProgress;
};

struct FoldingRangeRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
    std::optional<std::string> id;
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

struct MonikerOptions {
    std::optional<bool> workDoneProgress;
};

struct MonikerRegistrationOptions {
    std::optional<DocumentSelector> documentSelector;
    std::optional<bool> workDoneProgress;
};

struct LspLiteral41 {
    std::string text;
};

using TextDocumentContentChangeEvent = std::variant<LspLiteral40, LspLiteral41>;

struct Unregistration {
    std::string id;
    std::string method;
};

struct UnregistrationParams {
    std::vector<Unregistration> unregisterations;
};

struct DiagnosticClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<bool> relatedDocumentSupport;
};

struct LspLiteral7 {
    std::optional<bool> delta;
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

struct VersionedNotebookDocumentIdentifier {
    std::int32_t version;
    URI uri;
};

struct TextDocumentIdentifier {
    DocumentUri uri;
};

struct RenameParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Position position;
    std::string newName;
};

struct OptionalVersionedTextDocumentIdentifier {
    DocumentUri uri;
    std::optional<std::int32_t> version;
};

struct TextDocumentEdit {
    OptionalVersionedTextDocumentIdentifier textDocument;
    std::vector<std::variant<TextEdit, AnnotatedTextEdit>> edits;
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

struct DocumentColorParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct DocumentDiagnosticParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    std::optional<std::string> identifier;
    std::optional<std::string> previousResultId;
};

struct SemanticTokensParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct DidSaveTextDocumentParams {
    TextDocumentIdentifier textDocument;
    std::optional<std::string> text;
};

struct ColorPresentationParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    Color color;
    Range range;
};

struct WillSaveTextDocumentParams {
    TextDocumentIdentifier textDocument;
    TextDocumentSaveReason reason;
};

struct CodeLensParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct DocumentRangeFormattingParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Range range;
    FormattingOptions options;
};

struct SemanticTokensRangeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    Range range;
};

struct DocumentOnTypeFormattingParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::string ch;
    FormattingOptions options;
};

struct SemanticTokensDeltaParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    std::string previousResultId;
};

struct InlayHintParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Range range;
};

struct SelectionRangeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    std::vector<Position> positions;
};

struct TextDocumentPositionParams {
    TextDocumentIdentifier textDocument;
    Position position;
};

struct DefinitionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct PrepareRenameParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct TypeDefinitionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct HoverParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct CompletionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    std::optional<CompletionContext> context;
};

struct ImplementationParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct TypeHierarchyPrepareParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct CallHierarchyPrepareParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct SignatureHelpParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<SignatureHelpContext> context;
};

struct InlineCompletionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    InlineCompletionContext context;
};

struct LinkedEditingRangeParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
};

struct DocumentHighlightParams {
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

struct MonikerParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct DeclarationParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
};

struct InlineValueParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    Range range;
    InlineValueContext context;
};

struct DocumentFormattingParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    FormattingOptions options;
};

struct DidCloseTextDocumentParams {
    TextDocumentIdentifier textDocument;
};

struct CodeActionParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
    Range range;
    CodeActionContext context;
};

struct DocumentLinkParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct VersionedTextDocumentIdentifier {
    DocumentUri uri;
    std::int32_t version;
};

struct LspLiteral10 {
    VersionedTextDocumentIdentifier document;
    std::vector<TextDocumentContentChangeEvent> changes;
};

struct DidChangeTextDocumentParams {
    VersionedTextDocumentIdentifier textDocument;
    std::vector<TextDocumentContentChangeEvent> contentChanges;
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

struct DidChangeNotebookDocumentParams {
    VersionedNotebookDocumentIdentifier notebookDocument;
    NotebookDocumentChangeEvent change;
};

struct DocumentRangesFormattingParams {
    std::optional<ProgressToken> workDoneToken;
    TextDocumentIdentifier textDocument;
    std::vector<Range> ranges;
    FormattingOptions options;
};

struct DocumentSymbolParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct FoldingRangeParams {
    std::optional<ProgressToken> workDoneToken;
    std::optional<ProgressToken> partialResultToken;
    TextDocumentIdentifier textDocument;
};

struct DidCloseNotebookDocumentParams {
    NotebookDocumentIdentifier notebookDocument;
    std::vector<TextDocumentIdentifier> cellTextDocuments;
};

struct MarkdownClientCapabilities {
    std::string parser;
    std::optional<std::string> version;
    std::optional<std::vector<std::string>> allowedTags;
};

struct ApplyWorkspaceEditResult {
    bool applied;
    std::optional<std::string> failureReason;
    std::optional<std::uint32_t> failedChange;
};

struct MonikerClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct LspLiteral24 {
    std::vector<std::string> properties;
};

struct InlayHintClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral24> resolveSupport;
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

struct CompletionClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral22> completionItem;
    std::optional<LspLiteral26> completionItemKind;
    std::optional<InsertTextMode> insertTextMode;
    std::optional<bool> contextSupport;
    std::optional<LspLiteral27> completionList;
};

struct FileRename {
    std::string oldUri;
    std::string newUri;
};

struct RenameFilesParams {
    std::vector<FileRename> files;
};

struct InlineCompletionClientCapabilities {
    std::optional<bool> dynamicRegistration;
};

struct RegularExpressionsClientCapabilities {
    std::string engine;
    std::optional<std::string> version;
};

struct LspLiteral17 {
    bool cancel;
    std::vector<std::string> retryOnContentModified;
};

struct GeneralClientCapabilities {
    std::optional<LspLiteral17> staleRequestSupport;
    std::optional<RegularExpressionsClientCapabilities> regularExpressions;
    std::optional<MarkdownClientCapabilities> markdown;
    std::optional<std::vector<PositionEncodingKind>> positionEncodings;
};

struct InlineValueWorkspaceClientCapabilities {
    std::optional<bool> refreshSupport;
};

struct InitializedParams {};

struct DidChangeConfigurationParams {
    LSPAny settings;
};

struct LspLiteral20 {
    std::vector<SymbolTag> valueSet;
};

struct WorkspaceSymbolClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral19> symbolKind;
    std::optional<LspLiteral20> tagSupport;
    std::optional<LspLiteral21> resolveSupport;
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

struct DocumentSymbolClientCapabilities {
    std::optional<bool> dynamicRegistration;
    std::optional<LspLiteral19> symbolKind;
    std::optional<bool> hierarchicalDocumentSymbolSupport;
    std::optional<LspLiteral20> tagSupport;
    std::optional<bool> labelSupport;
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

struct SetTraceParams {
    TraceValues value;
};

struct WorkDoneProgressEnd {
    rfl::Literal<"end"> kind;
    std::optional<std::string> message;
};

}  // namespace eventide::language::proto

namespace rfl {

template <typename T>
struct Reflector;

template <>
struct Reflector<eventide::language::proto::SemanticTokenTypesEnum> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::SemanticTokenTypesEnum& value) {
        switch(value) {
            case eventide::language::proto::SemanticTokenTypesEnum::namespace_: return "namespace";
            case eventide::language::proto::SemanticTokenTypesEnum::type: return "type";
            case eventide::language::proto::SemanticTokenTypesEnum::class_: return "class";
            case eventide::language::proto::SemanticTokenTypesEnum::enum_: return "enum";
            case eventide::language::proto::SemanticTokenTypesEnum::interface: return "interface";
            case eventide::language::proto::SemanticTokenTypesEnum::struct_: return "struct";
            case eventide::language::proto::SemanticTokenTypesEnum::typeParameter:
                return "typeParameter";
            case eventide::language::proto::SemanticTokenTypesEnum::parameter: return "parameter";
            case eventide::language::proto::SemanticTokenTypesEnum::variable: return "variable";
            case eventide::language::proto::SemanticTokenTypesEnum::property: return "property";
            case eventide::language::proto::SemanticTokenTypesEnum::enumMember: return "enumMember";
            case eventide::language::proto::SemanticTokenTypesEnum::event: return "event";
            case eventide::language::proto::SemanticTokenTypesEnum::function: return "function";
            case eventide::language::proto::SemanticTokenTypesEnum::method: return "method";
            case eventide::language::proto::SemanticTokenTypesEnum::macro: return "macro";
            case eventide::language::proto::SemanticTokenTypesEnum::keyword: return "keyword";
            case eventide::language::proto::SemanticTokenTypesEnum::modifier: return "modifier";
            case eventide::language::proto::SemanticTokenTypesEnum::comment: return "comment";
            case eventide::language::proto::SemanticTokenTypesEnum::string: return "string";
            case eventide::language::proto::SemanticTokenTypesEnum::number: return "number";
            case eventide::language::proto::SemanticTokenTypesEnum::regexp: return "regexp";
            case eventide::language::proto::SemanticTokenTypesEnum::operator_: return "operator";
            case eventide::language::proto::SemanticTokenTypesEnum::decorator: return "decorator";
        }
        return {};
    }

    static eventide::language::proto::SemanticTokenTypesEnum to(const ReflType& value) {
        if(value == "namespace") {
            return eventide::language::proto::SemanticTokenTypesEnum::namespace_;
        }
        if(value == "type") {
            return eventide::language::proto::SemanticTokenTypesEnum::type;
        }
        if(value == "class") {
            return eventide::language::proto::SemanticTokenTypesEnum::class_;
        }
        if(value == "enum") {
            return eventide::language::proto::SemanticTokenTypesEnum::enum_;
        }
        if(value == "interface") {
            return eventide::language::proto::SemanticTokenTypesEnum::interface;
        }
        if(value == "struct") {
            return eventide::language::proto::SemanticTokenTypesEnum::struct_;
        }
        if(value == "typeParameter") {
            return eventide::language::proto::SemanticTokenTypesEnum::typeParameter;
        }
        if(value == "parameter") {
            return eventide::language::proto::SemanticTokenTypesEnum::parameter;
        }
        if(value == "variable") {
            return eventide::language::proto::SemanticTokenTypesEnum::variable;
        }
        if(value == "property") {
            return eventide::language::proto::SemanticTokenTypesEnum::property;
        }
        if(value == "enumMember") {
            return eventide::language::proto::SemanticTokenTypesEnum::enumMember;
        }
        if(value == "event") {
            return eventide::language::proto::SemanticTokenTypesEnum::event;
        }
        if(value == "function") {
            return eventide::language::proto::SemanticTokenTypesEnum::function;
        }
        if(value == "method") {
            return eventide::language::proto::SemanticTokenTypesEnum::method;
        }
        if(value == "macro") {
            return eventide::language::proto::SemanticTokenTypesEnum::macro;
        }
        if(value == "keyword") {
            return eventide::language::proto::SemanticTokenTypesEnum::keyword;
        }
        if(value == "modifier") {
            return eventide::language::proto::SemanticTokenTypesEnum::modifier;
        }
        if(value == "comment") {
            return eventide::language::proto::SemanticTokenTypesEnum::comment;
        }
        if(value == "string") {
            return eventide::language::proto::SemanticTokenTypesEnum::string;
        }
        if(value == "number") {
            return eventide::language::proto::SemanticTokenTypesEnum::number;
        }
        if(value == "regexp") {
            return eventide::language::proto::SemanticTokenTypesEnum::regexp;
        }
        if(value == "operator") {
            return eventide::language::proto::SemanticTokenTypesEnum::operator_;
        }
        if(value == "decorator") {
            return eventide::language::proto::SemanticTokenTypesEnum::decorator;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::SemanticTokenModifiersEnum> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::SemanticTokenModifiersEnum& value) {
        switch(value) {
            case eventide::language::proto::SemanticTokenModifiersEnum::declaration:
                return "declaration";
            case eventide::language::proto::SemanticTokenModifiersEnum::definition:
                return "definition";
            case eventide::language::proto::SemanticTokenModifiersEnum::readonly: return "readonly";
            case eventide::language::proto::SemanticTokenModifiersEnum::static_: return "static";
            case eventide::language::proto::SemanticTokenModifiersEnum::deprecated:
                return "deprecated";
            case eventide::language::proto::SemanticTokenModifiersEnum::abstract: return "abstract";
            case eventide::language::proto::SemanticTokenModifiersEnum::async: return "async";
            case eventide::language::proto::SemanticTokenModifiersEnum::modification:
                return "modification";
            case eventide::language::proto::SemanticTokenModifiersEnum::documentation:
                return "documentation";
            case eventide::language::proto::SemanticTokenModifiersEnum::defaultLibrary:
                return "defaultLibrary";
        }
        return {};
    }

    static eventide::language::proto::SemanticTokenModifiersEnum to(const ReflType& value) {
        if(value == "declaration") {
            return eventide::language::proto::SemanticTokenModifiersEnum::declaration;
        }
        if(value == "definition") {
            return eventide::language::proto::SemanticTokenModifiersEnum::definition;
        }
        if(value == "readonly") {
            return eventide::language::proto::SemanticTokenModifiersEnum::readonly;
        }
        if(value == "static") {
            return eventide::language::proto::SemanticTokenModifiersEnum::static_;
        }
        if(value == "deprecated") {
            return eventide::language::proto::SemanticTokenModifiersEnum::deprecated;
        }
        if(value == "abstract") {
            return eventide::language::proto::SemanticTokenModifiersEnum::abstract;
        }
        if(value == "async") {
            return eventide::language::proto::SemanticTokenModifiersEnum::async;
        }
        if(value == "modification") {
            return eventide::language::proto::SemanticTokenModifiersEnum::modification;
        }
        if(value == "documentation") {
            return eventide::language::proto::SemanticTokenModifiersEnum::documentation;
        }
        if(value == "defaultLibrary") {
            return eventide::language::proto::SemanticTokenModifiersEnum::defaultLibrary;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::DocumentDiagnosticReportKind> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::DocumentDiagnosticReportKind& value) {
        switch(value) {
            case eventide::language::proto::DocumentDiagnosticReportKind::Full: return "full";
            case eventide::language::proto::DocumentDiagnosticReportKind::Unchanged:
                return "unchanged";
        }
        return {};
    }

    static eventide::language::proto::DocumentDiagnosticReportKind to(const ReflType& value) {
        if(value == "full") {
            return eventide::language::proto::DocumentDiagnosticReportKind::Full;
        }
        if(value == "unchanged") {
            return eventide::language::proto::DocumentDiagnosticReportKind::Unchanged;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::ErrorCodes> {
    using ReflType = std::int32_t;

    static ReflType from(const eventide::language::proto::ErrorCodes& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::ErrorCodes to(const ReflType& value) {
        return static_cast<eventide::language::proto::ErrorCodes>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::LSPErrorCodes> {
    using ReflType = std::int32_t;

    static ReflType from(const eventide::language::proto::LSPErrorCodes& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::LSPErrorCodes to(const ReflType& value) {
        return static_cast<eventide::language::proto::LSPErrorCodes>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::FoldingRangeKindEnum> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::FoldingRangeKindEnum& value) {
        switch(value) {
            case eventide::language::proto::FoldingRangeKindEnum::Comment: return "comment";
            case eventide::language::proto::FoldingRangeKindEnum::Imports: return "imports";
            case eventide::language::proto::FoldingRangeKindEnum::Region: return "region";
        }
        return {};
    }

    static eventide::language::proto::FoldingRangeKindEnum to(const ReflType& value) {
        if(value == "comment") {
            return eventide::language::proto::FoldingRangeKindEnum::Comment;
        }
        if(value == "imports") {
            return eventide::language::proto::FoldingRangeKindEnum::Imports;
        }
        if(value == "region") {
            return eventide::language::proto::FoldingRangeKindEnum::Region;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::SymbolKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::SymbolKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::SymbolKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::SymbolKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::SymbolTag> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::SymbolTag& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::SymbolTag to(const ReflType& value) {
        return static_cast<eventide::language::proto::SymbolTag>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::UniquenessLevel> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::UniquenessLevel& value) {
        switch(value) {
            case eventide::language::proto::UniquenessLevel::document: return "document";
            case eventide::language::proto::UniquenessLevel::project: return "project";
            case eventide::language::proto::UniquenessLevel::group: return "group";
            case eventide::language::proto::UniquenessLevel::scheme: return "scheme";
            case eventide::language::proto::UniquenessLevel::global: return "global";
        }
        return {};
    }

    static eventide::language::proto::UniquenessLevel to(const ReflType& value) {
        if(value == "document") {
            return eventide::language::proto::UniquenessLevel::document;
        }
        if(value == "project") {
            return eventide::language::proto::UniquenessLevel::project;
        }
        if(value == "group") {
            return eventide::language::proto::UniquenessLevel::group;
        }
        if(value == "scheme") {
            return eventide::language::proto::UniquenessLevel::scheme;
        }
        if(value == "global") {
            return eventide::language::proto::UniquenessLevel::global;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::MonikerKind> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::MonikerKind& value) {
        switch(value) {
            case eventide::language::proto::MonikerKind::import: return "import";
            case eventide::language::proto::MonikerKind::export_: return "export";
            case eventide::language::proto::MonikerKind::local: return "local";
        }
        return {};
    }

    static eventide::language::proto::MonikerKind to(const ReflType& value) {
        if(value == "import") {
            return eventide::language::proto::MonikerKind::import;
        }
        if(value == "export") {
            return eventide::language::proto::MonikerKind::export_;
        }
        if(value == "local") {
            return eventide::language::proto::MonikerKind::local;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::InlayHintKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::InlayHintKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::InlayHintKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::InlayHintKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::MessageType> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::MessageType& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::MessageType to(const ReflType& value) {
        return static_cast<eventide::language::proto::MessageType>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::TextDocumentSyncKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::TextDocumentSyncKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::TextDocumentSyncKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::TextDocumentSyncKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::TextDocumentSaveReason> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::TextDocumentSaveReason& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::TextDocumentSaveReason to(const ReflType& value) {
        return static_cast<eventide::language::proto::TextDocumentSaveReason>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::CompletionItemKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::CompletionItemKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::CompletionItemKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::CompletionItemKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::CompletionItemTag> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::CompletionItemTag& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::CompletionItemTag to(const ReflType& value) {
        return static_cast<eventide::language::proto::CompletionItemTag>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::InsertTextFormat> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::InsertTextFormat& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::InsertTextFormat to(const ReflType& value) {
        return static_cast<eventide::language::proto::InsertTextFormat>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::InsertTextMode> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::InsertTextMode& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::InsertTextMode to(const ReflType& value) {
        return static_cast<eventide::language::proto::InsertTextMode>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::DocumentHighlightKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::DocumentHighlightKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::DocumentHighlightKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::DocumentHighlightKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::CodeActionKindEnum> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::CodeActionKindEnum& value) {
        switch(value) {
            case eventide::language::proto::CodeActionKindEnum::Empty: return "";
            case eventide::language::proto::CodeActionKindEnum::QuickFix: return "quickfix";
            case eventide::language::proto::CodeActionKindEnum::Refactor: return "refactor";
            case eventide::language::proto::CodeActionKindEnum::RefactorExtract:
                return "refactor.extract";
            case eventide::language::proto::CodeActionKindEnum::RefactorInline:
                return "refactor.inline";
            case eventide::language::proto::CodeActionKindEnum::RefactorRewrite:
                return "refactor.rewrite";
            case eventide::language::proto::CodeActionKindEnum::Source: return "source";
            case eventide::language::proto::CodeActionKindEnum::SourceOrganizeImports:
                return "source.organizeImports";
            case eventide::language::proto::CodeActionKindEnum::SourceFixAll:
                return "source.fixAll";
        }
        return {};
    }

    static eventide::language::proto::CodeActionKindEnum to(const ReflType& value) {
        if(value == "") {
            return eventide::language::proto::CodeActionKindEnum::Empty;
        }
        if(value == "quickfix") {
            return eventide::language::proto::CodeActionKindEnum::QuickFix;
        }
        if(value == "refactor") {
            return eventide::language::proto::CodeActionKindEnum::Refactor;
        }
        if(value == "refactor.extract") {
            return eventide::language::proto::CodeActionKindEnum::RefactorExtract;
        }
        if(value == "refactor.inline") {
            return eventide::language::proto::CodeActionKindEnum::RefactorInline;
        }
        if(value == "refactor.rewrite") {
            return eventide::language::proto::CodeActionKindEnum::RefactorRewrite;
        }
        if(value == "source") {
            return eventide::language::proto::CodeActionKindEnum::Source;
        }
        if(value == "source.organizeImports") {
            return eventide::language::proto::CodeActionKindEnum::SourceOrganizeImports;
        }
        if(value == "source.fixAll") {
            return eventide::language::proto::CodeActionKindEnum::SourceFixAll;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::TraceValues> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::TraceValues& value) {
        switch(value) {
            case eventide::language::proto::TraceValues::Off: return "off";
            case eventide::language::proto::TraceValues::Messages: return "messages";
            case eventide::language::proto::TraceValues::Verbose: return "verbose";
        }
        return {};
    }

    static eventide::language::proto::TraceValues to(const ReflType& value) {
        if(value == "off") {
            return eventide::language::proto::TraceValues::Off;
        }
        if(value == "messages") {
            return eventide::language::proto::TraceValues::Messages;
        }
        if(value == "verbose") {
            return eventide::language::proto::TraceValues::Verbose;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::MarkupKind> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::MarkupKind& value) {
        switch(value) {
            case eventide::language::proto::MarkupKind::PlainText: return "plaintext";
            case eventide::language::proto::MarkupKind::Markdown: return "markdown";
        }
        return {};
    }

    static eventide::language::proto::MarkupKind to(const ReflType& value) {
        if(value == "plaintext") {
            return eventide::language::proto::MarkupKind::PlainText;
        }
        if(value == "markdown") {
            return eventide::language::proto::MarkupKind::Markdown;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::InlineCompletionTriggerKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::InlineCompletionTriggerKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::InlineCompletionTriggerKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::InlineCompletionTriggerKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::PositionEncodingKindEnum> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::PositionEncodingKindEnum& value) {
        switch(value) {
            case eventide::language::proto::PositionEncodingKindEnum::UTF8: return "utf-8";
            case eventide::language::proto::PositionEncodingKindEnum::UTF16: return "utf-16";
            case eventide::language::proto::PositionEncodingKindEnum::UTF32: return "utf-32";
        }
        return {};
    }

    static eventide::language::proto::PositionEncodingKindEnum to(const ReflType& value) {
        if(value == "utf-8") {
            return eventide::language::proto::PositionEncodingKindEnum::UTF8;
        }
        if(value == "utf-16") {
            return eventide::language::proto::PositionEncodingKindEnum::UTF16;
        }
        if(value == "utf-32") {
            return eventide::language::proto::PositionEncodingKindEnum::UTF32;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::FileChangeType> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::FileChangeType& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::FileChangeType to(const ReflType& value) {
        return static_cast<eventide::language::proto::FileChangeType>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::WatchKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::WatchKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::WatchKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::WatchKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::DiagnosticSeverity> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::DiagnosticSeverity& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::DiagnosticSeverity to(const ReflType& value) {
        return static_cast<eventide::language::proto::DiagnosticSeverity>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::DiagnosticTag> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::DiagnosticTag& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::DiagnosticTag to(const ReflType& value) {
        return static_cast<eventide::language::proto::DiagnosticTag>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::CompletionTriggerKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::CompletionTriggerKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::CompletionTriggerKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::CompletionTriggerKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::SignatureHelpTriggerKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::SignatureHelpTriggerKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::SignatureHelpTriggerKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::SignatureHelpTriggerKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::CodeActionTriggerKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::CodeActionTriggerKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::CodeActionTriggerKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::CodeActionTriggerKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::FileOperationPatternKind> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::FileOperationPatternKind& value) {
        switch(value) {
            case eventide::language::proto::FileOperationPatternKind::file: return "file";
            case eventide::language::proto::FileOperationPatternKind::folder: return "folder";
        }
        return {};
    }

    static eventide::language::proto::FileOperationPatternKind to(const ReflType& value) {
        if(value == "file") {
            return eventide::language::proto::FileOperationPatternKind::file;
        }
        if(value == "folder") {
            return eventide::language::proto::FileOperationPatternKind::folder;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::NotebookCellKind> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::NotebookCellKind& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::NotebookCellKind to(const ReflType& value) {
        return static_cast<eventide::language::proto::NotebookCellKind>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::ResourceOperationKind> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::ResourceOperationKind& value) {
        switch(value) {
            case eventide::language::proto::ResourceOperationKind::Create: return "create";
            case eventide::language::proto::ResourceOperationKind::Rename: return "rename";
            case eventide::language::proto::ResourceOperationKind::Delete: return "delete";
        }
        return {};
    }

    static eventide::language::proto::ResourceOperationKind to(const ReflType& value) {
        if(value == "create") {
            return eventide::language::proto::ResourceOperationKind::Create;
        }
        if(value == "rename") {
            return eventide::language::proto::ResourceOperationKind::Rename;
        }
        if(value == "delete") {
            return eventide::language::proto::ResourceOperationKind::Delete;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::FailureHandlingKind> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::FailureHandlingKind& value) {
        switch(value) {
            case eventide::language::proto::FailureHandlingKind::Abort: return "abort";
            case eventide::language::proto::FailureHandlingKind::Transactional:
                return "transactional";
            case eventide::language::proto::FailureHandlingKind::TextOnlyTransactional:
                return "textOnlyTransactional";
            case eventide::language::proto::FailureHandlingKind::Undo: return "undo";
        }
        return {};
    }

    static eventide::language::proto::FailureHandlingKind to(const ReflType& value) {
        if(value == "abort") {
            return eventide::language::proto::FailureHandlingKind::Abort;
        }
        if(value == "transactional") {
            return eventide::language::proto::FailureHandlingKind::Transactional;
        }
        if(value == "textOnlyTransactional") {
            return eventide::language::proto::FailureHandlingKind::TextOnlyTransactional;
        }
        if(value == "undo") {
            return eventide::language::proto::FailureHandlingKind::Undo;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

template <>
struct Reflector<eventide::language::proto::PrepareSupportDefaultBehavior> {
    using ReflType = std::uint32_t;

    static ReflType from(const eventide::language::proto::PrepareSupportDefaultBehavior& value) {
        return static_cast<ReflType>(value);
    }

    static eventide::language::proto::PrepareSupportDefaultBehavior to(const ReflType& value) {
        return static_cast<eventide::language::proto::PrepareSupportDefaultBehavior>(value);
    }
};

template <>
struct Reflector<eventide::language::proto::TokenFormat> {
    using ReflType = std::string;

    static ReflType from(const eventide::language::proto::TokenFormat& value) {
        switch(value) {
            case eventide::language::proto::TokenFormat::Relative: return "relative";
        }
        return {};
    }

    static eventide::language::proto::TokenFormat to(const ReflType& value) {
        if(value == "relative") {
            return eventide::language::proto::TokenFormat::Relative;
        }
        throw std::runtime_error("Unknown enum value");
    }
};

}  // namespace rfl
