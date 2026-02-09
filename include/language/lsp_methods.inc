#pragma once

#include <string_view>

#define EVENTIDE_LSP_REQUESTS(X) \
    X(text_document_implementation, "textDocument/implementation", (ImplementationParams), (std::optional<std::variant<Definition, std::vector<DefinitionLink>>>)) \
    X(text_document_type_definition, "textDocument/typeDefinition", (TypeDefinitionParams), (std::optional<std::variant<Definition, std::vector<DefinitionLink>>>)) \
    X(workspace_workspace_folders, "workspace/workspaceFolders", (eventide::language::proto::no_params), (std::optional<std::vector<WorkspaceFolder>>)) \
    X(workspace_configuration, "workspace/configuration", (ConfigurationParams), (std::vector<LSPAny>)) \
    X(text_document_document_color, "textDocument/documentColor", (DocumentColorParams), (std::vector<ColorInformation>)) \
    X(text_document_color_presentation, "textDocument/colorPresentation", (ColorPresentationParams), (std::vector<ColorPresentation>)) \
    X(text_document_folding_range, "textDocument/foldingRange", (FoldingRangeParams), (std::optional<std::vector<FoldingRange>>)) \
    X(workspace_folding_range_refresh, "workspace/foldingRange/refresh", (eventide::language::proto::no_params), (LSPAny)) \
    X(text_document_declaration, "textDocument/declaration", (DeclarationParams), (std::optional<std::variant<Declaration, std::vector<DeclarationLink>>>)) \
    X(text_document_selection_range, "textDocument/selectionRange", (SelectionRangeParams), (std::optional<std::vector<SelectionRange>>)) \
    X(window_work_done_progress_create, "window/workDoneProgress/create", (WorkDoneProgressCreateParams), (LSPAny)) \
    X(text_document_prepare_call_hierarchy, "textDocument/prepareCallHierarchy", (CallHierarchyPrepareParams), (std::optional<std::vector<CallHierarchyItem>>)) \
    X(call_hierarchy_incoming_calls, "callHierarchy/incomingCalls", (CallHierarchyIncomingCallsParams), (std::optional<std::vector<CallHierarchyIncomingCall>>)) \
    X(call_hierarchy_outgoing_calls, "callHierarchy/outgoingCalls", (CallHierarchyOutgoingCallsParams), (std::optional<std::vector<CallHierarchyOutgoingCall>>)) \
    X(text_document_semantic_tokens_full, "textDocument/semanticTokens/full", (SemanticTokensParams), (std::optional<SemanticTokens>)) \
    X(text_document_semantic_tokens_full_delta, "textDocument/semanticTokens/full/delta", (SemanticTokensDeltaParams), (std::optional<std::variant<SemanticTokens, SemanticTokensDelta>>)) \
    X(text_document_semantic_tokens_range, "textDocument/semanticTokens/range", (SemanticTokensRangeParams), (std::optional<SemanticTokens>)) \
    X(workspace_semantic_tokens_refresh, "workspace/semanticTokens/refresh", (eventide::language::proto::no_params), (LSPAny)) \
    X(window_show_document, "window/showDocument", (ShowDocumentParams), (ShowDocumentResult)) \
    X(text_document_linked_editing_range, "textDocument/linkedEditingRange", (LinkedEditingRangeParams), (std::optional<LinkedEditingRanges>)) \
    X(workspace_will_create_files, "workspace/willCreateFiles", (CreateFilesParams), (std::optional<WorkspaceEdit>)) \
    X(workspace_will_rename_files, "workspace/willRenameFiles", (RenameFilesParams), (std::optional<WorkspaceEdit>)) \
    X(workspace_will_delete_files, "workspace/willDeleteFiles", (DeleteFilesParams), (std::optional<WorkspaceEdit>)) \
    X(text_document_moniker, "textDocument/moniker", (MonikerParams), (std::optional<std::vector<Moniker>>)) \
    X(text_document_prepare_type_hierarchy, "textDocument/prepareTypeHierarchy", (TypeHierarchyPrepareParams), (std::optional<std::vector<TypeHierarchyItem>>)) \
    X(type_hierarchy_supertypes, "typeHierarchy/supertypes", (TypeHierarchySupertypesParams), (std::optional<std::vector<TypeHierarchyItem>>)) \
    X(type_hierarchy_subtypes, "typeHierarchy/subtypes", (TypeHierarchySubtypesParams), (std::optional<std::vector<TypeHierarchyItem>>)) \
    X(text_document_inline_value, "textDocument/inlineValue", (InlineValueParams), (std::optional<std::vector<InlineValue>>)) \
    X(workspace_inline_value_refresh, "workspace/inlineValue/refresh", (eventide::language::proto::no_params), (LSPAny)) \
    X(text_document_inlay_hint, "textDocument/inlayHint", (InlayHintParams), (std::optional<std::vector<InlayHint>>)) \
    X(inlay_hint_resolve, "inlayHint/resolve", (InlayHint), (InlayHint)) \
    X(workspace_inlay_hint_refresh, "workspace/inlayHint/refresh", (eventide::language::proto::no_params), (LSPAny)) \
    X(text_document_diagnostic, "textDocument/diagnostic", (DocumentDiagnosticParams), (DocumentDiagnosticReport)) \
    X(workspace_diagnostic, "workspace/diagnostic", (WorkspaceDiagnosticParams), (WorkspaceDiagnosticReport)) \
    X(workspace_diagnostic_refresh, "workspace/diagnostic/refresh", (eventide::language::proto::no_params), (LSPAny)) \
    X(text_document_inline_completion, "textDocument/inlineCompletion", (InlineCompletionParams), (std::optional<std::variant<InlineCompletionList, std::vector<InlineCompletionItem>>>)) \
    X(client_register_capability, "client/registerCapability", (RegistrationParams), (LSPAny)) \
    X(client_unregister_capability, "client/unregisterCapability", (UnregistrationParams), (LSPAny)) \
    X(initialize, "initialize", (InitializeParams), (InitializeResult)) \
    X(shutdown, "shutdown", (eventide::language::proto::no_params), (LSPAny)) \
    X(window_show_message_request, "window/showMessageRequest", (ShowMessageRequestParams), (std::optional<MessageActionItem>)) \
    X(text_document_will_save_wait_until, "textDocument/willSaveWaitUntil", (WillSaveTextDocumentParams), (std::optional<std::vector<TextEdit>>)) \
    X(text_document_completion, "textDocument/completion", (CompletionParams), (std::optional<std::variant<std::vector<CompletionItem>, CompletionList>>)) \
    X(completion_item_resolve, "completionItem/resolve", (CompletionItem), (CompletionItem)) \
    X(text_document_hover, "textDocument/hover", (HoverParams), (std::optional<Hover>)) \
    X(text_document_signature_help, "textDocument/signatureHelp", (SignatureHelpParams), (std::optional<SignatureHelp>)) \
    X(text_document_definition, "textDocument/definition", (DefinitionParams), (std::optional<std::variant<Definition, std::vector<DefinitionLink>>>)) \
    X(text_document_references, "textDocument/references", (ReferenceParams), (std::optional<std::vector<Location>>)) \
    X(text_document_document_highlight, "textDocument/documentHighlight", (DocumentHighlightParams), (std::optional<std::vector<DocumentHighlight>>)) \
    X(text_document_document_symbol, "textDocument/documentSymbol", (DocumentSymbolParams), (std::optional<std::variant<std::vector<SymbolInformation>, std::vector<DocumentSymbol>>>)) \
    X(text_document_code_action, "textDocument/codeAction", (CodeActionParams), (std::optional<std::vector<std::variant<Command, CodeAction>>>)) \
    X(code_action_resolve, "codeAction/resolve", (CodeAction), (CodeAction)) \
    X(workspace_symbol, "workspace/symbol", (WorkspaceSymbolParams), (std::optional<std::variant<std::vector<SymbolInformation>, std::vector<WorkspaceSymbol>>>)) \
    X(workspace_symbol_resolve, "workspaceSymbol/resolve", (WorkspaceSymbol), (WorkspaceSymbol)) \
    X(text_document_code_lens, "textDocument/codeLens", (CodeLensParams), (std::optional<std::vector<CodeLens>>)) \
    X(code_lens_resolve, "codeLens/resolve", (CodeLens), (CodeLens)) \
    X(workspace_code_lens_refresh, "workspace/codeLens/refresh", (eventide::language::proto::no_params), (LSPAny)) \
    X(text_document_document_link, "textDocument/documentLink", (DocumentLinkParams), (std::optional<std::vector<DocumentLink>>)) \
    X(document_link_resolve, "documentLink/resolve", (DocumentLink), (DocumentLink)) \
    X(text_document_formatting, "textDocument/formatting", (DocumentFormattingParams), (std::optional<std::vector<TextEdit>>)) \
    X(text_document_range_formatting, "textDocument/rangeFormatting", (DocumentRangeFormattingParams), (std::optional<std::vector<TextEdit>>)) \
    X(text_document_ranges_formatting, "textDocument/rangesFormatting", (DocumentRangesFormattingParams), (std::optional<std::vector<TextEdit>>)) \
    X(text_document_on_type_formatting, "textDocument/onTypeFormatting", (DocumentOnTypeFormattingParams), (std::optional<std::vector<TextEdit>>)) \
    X(text_document_rename, "textDocument/rename", (RenameParams), (std::optional<WorkspaceEdit>)) \
    X(text_document_prepare_rename, "textDocument/prepareRename", (PrepareRenameParams), (std::optional<PrepareRenameResult>)) \
    X(workspace_execute_command, "workspace/executeCommand", (ExecuteCommandParams), (std::optional<LSPAny>)) \
    X(workspace_apply_edit, "workspace/applyEdit", (ApplyWorkspaceEditParams), (ApplyWorkspaceEditResult))

#define EVENTIDE_LSP_NOTIFICATIONS(X) \
    X(workspace_did_change_workspace_folders, "workspace/didChangeWorkspaceFolders", (DidChangeWorkspaceFoldersParams)) \
    X(window_work_done_progress_cancel, "window/workDoneProgress/cancel", (WorkDoneProgressCancelParams)) \
    X(workspace_did_create_files, "workspace/didCreateFiles", (CreateFilesParams)) \
    X(workspace_did_rename_files, "workspace/didRenameFiles", (RenameFilesParams)) \
    X(workspace_did_delete_files, "workspace/didDeleteFiles", (DeleteFilesParams)) \
    X(notebook_document_did_open, "notebookDocument/didOpen", (DidOpenNotebookDocumentParams)) \
    X(notebook_document_did_change, "notebookDocument/didChange", (DidChangeNotebookDocumentParams)) \
    X(notebook_document_did_save, "notebookDocument/didSave", (DidSaveNotebookDocumentParams)) \
    X(notebook_document_did_close, "notebookDocument/didClose", (DidCloseNotebookDocumentParams)) \
    X(initialized, "initialized", (InitializedParams)) \
    X(exit, "exit", (eventide::language::proto::no_params)) \
    X(workspace_did_change_configuration, "workspace/didChangeConfiguration", (DidChangeConfigurationParams)) \
    X(window_show_message, "window/showMessage", (ShowMessageParams)) \
    X(window_log_message, "window/logMessage", (LogMessageParams)) \
    X(telemetry_event, "telemetry/event", (LSPAny)) \
    X(text_document_did_open, "textDocument/didOpen", (DidOpenTextDocumentParams)) \
    X(text_document_did_change, "textDocument/didChange", (DidChangeTextDocumentParams)) \
    X(text_document_did_close, "textDocument/didClose", (DidCloseTextDocumentParams)) \
    X(text_document_did_save, "textDocument/didSave", (DidSaveTextDocumentParams)) \
    X(text_document_will_save, "textDocument/willSave", (WillSaveTextDocumentParams)) \
    X(workspace_did_change_watched_files, "workspace/didChangeWatchedFiles", (DidChangeWatchedFilesParams)) \
    X(text_document_publish_diagnostics, "textDocument/publishDiagnostics", (PublishDiagnosticsParams)) \
    X(__set_trace, "$/setTrace", (SetTraceParams)) \
    X(__log_trace, "$/logTrace", (LogTraceParams)) \
    X(__cancel_request, "$/cancelRequest", (CancelParams)) \
    X(__progress, "$/progress", (ProgressParams))
