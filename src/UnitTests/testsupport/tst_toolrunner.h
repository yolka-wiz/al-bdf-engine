// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (c) 2026 albdf contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// This file is part of the albdf project, a fork of PDF4QT (MIT).
// The upstream PDF4QT portions remain under the MIT License; see the
// upstream copyright headers and the LICENSE file.

// Shared QProcess harness for integration tests that shell out to `albdf`.
// Before R2.1 every suite copy-pasted its own `runTool` with subtly different
// sentinels/timeouts; routing them through one helper keeps the CLI-invocation
// semantics (headless env, separate channels, crash/timeout reporting) identical
// across the suite.

#pragma once

#include <QByteArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace testsupport
{

/// Outcome of a single `albdf` invocation.
struct ToolResult
{
    /// Process exit code, or a negative sentinel when the process never
    /// reported one: -100 = could not start, -101 = timed out and was killed.
    int exitCode = -100;
    /// How the process ended; only meaningful when `finishedInTime` is true.
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    /// False when the child did not finish within the timeout.
    bool finishedInTime = false;
    QByteArray stdoutData;
    QByteArray stderrData;
};

/// Runs `toolPath` with `arguments` in `workDir` and captures its output.
///
/// The child is always headless (`QT_QPA_PLATFORM=offscreen`, so the helper is
/// safe outside ctest too) with separate stdout/stderr channels. On timeout it
/// is killed and `exitCode` is -101; `finishedInTime` stays false. If it cannot
/// be started, `exitCode` stays -100.
///
/// \param toolPath path to the albdf executable.
/// \param arguments command-line arguments, the command name first.
/// \param workDir working directory for the child process.
/// \param timeoutMs per-invocation watchdog in milliseconds.
/// \returns the captured exit code, exit status and output streams.
inline ToolResult
runAlbdfTool(const QString& toolPath, const QStringList& arguments, const QString& workDir, int timeoutMs = 60000)
{
    ToolResult result;
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(workDir);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(toolPath, arguments);
    if (!process.waitForStarted())
    {
        return result;
    }
    result.finishedInTime = process.waitForFinished(timeoutMs);
    if (!result.finishedInTime)
    {
        process.kill();
        process.waitForFinished(5000);
        result.exitCode = -101;
        return result;
    }
    result.exitStatus = process.exitStatus();
    result.exitCode = process.exitCode();
    result.stdoutData = process.readAllStandardOutput();
    result.stderrData = process.readAllStandardError();
    return result;
}

} // namespace testsupport
