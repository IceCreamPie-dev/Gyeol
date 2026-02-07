import * as vscode from 'vscode';
import * as path from 'path';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext): void {
    const serverPath = resolveServerPath();

    if (!serverPath) {
        vscode.window.showWarningMessage(
            'GyeolLSP executable not found. Set "gyeol.lsp.path" in settings or add GyeolLSP to your PATH. ' +
            'Syntax highlighting is still active, but language intelligence features are disabled.'
        );
        return;
    }

    const serverOptions: ServerOptions = {
        run: {
            command: serverPath,
            transport: TransportKind.stdio,
        },
        debug: {
            command: serverPath,
            transport: TransportKind.stdio,
        },
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [
            { scheme: 'file', language: 'gyeol' },
        ],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.gyeol'),
        },
    };

    client = new LanguageClient(
        'gyeolLSP',
        'Gyeol Language Server',
        serverOptions,
        clientOptions
    );

    client.start();
    context.subscriptions.push({
        dispose: () => {
            if (client) {
                client.stop();
            }
        },
    });

    const outputChannel = vscode.window.createOutputChannel('Gyeol');
    outputChannel.appendLine(`Gyeol Language Server started: ${serverPath}`);
    context.subscriptions.push(outputChannel);
}

export function deactivate(): Thenable<void> | undefined {
    if (client) {
        return client.stop();
    }
    return undefined;
}

function resolveServerPath(): string | undefined {
    const config = vscode.workspace.getConfiguration('gyeol');
    const configPath = config.get<string>('lsp.path');

    if (configPath && configPath.trim().length > 0) {
        return configPath.trim();
    }

    // Fall back to searching PATH for the GyeolLSP executable.
    // The LanguageClient will attempt to resolve the command via PATH
    // when given a bare executable name.
    const execName = process.platform === 'win32' ? 'GyeolLSP.exe' : 'GyeolLSP';

    // Check common project-relative locations first
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (workspaceFolders) {
        const candidates = [
            path.join(workspaceFolders[0].uri.fsPath, 'build', 'src', 'gyeol_lsp', execName),
            path.join(workspaceFolders[0].uri.fsPath, 'build', execName),
            path.join(workspaceFolders[0].uri.fsPath, 'bin', execName),
        ];
        for (const candidate of candidates) {
            // We return the first candidate path; the LanguageClient will
            // report an error if the file does not exist at runtime.
            // A more robust implementation could use fs.existsSync, but we
            // keep this scaffolding simple.
            try {
                const fs = require('fs');
                if (fs.existsSync(candidate)) {
                    return candidate;
                }
            } catch {
                // Ignore filesystem errors
            }
        }
    }

    // Fall back to bare executable name (resolved via PATH by the OS)
    return execName;
}
