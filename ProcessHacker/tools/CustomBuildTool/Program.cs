/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     dmex
 *
 */

namespace CustomBuildTool
{
    public static class Program
    {
        private static Dictionary<string, string> ProgramArgs;

        public static void Main(string[] args)
        {
            ProgramArgs = ParseArgs(args);

            if (!Build.InitializeBuildEnvironment())
                return;

            if (ProgramArgs.ContainsKey("-cleanup"))
            {
                Build.CleanupBuildEnvironment();
                Build.ShowBuildStats();
            }
            else if (ProgramArgs.ContainsKey("-encrypt"))
            {
                Verify.Encrypt(ProgramArgs["-input"], ProgramArgs["-output"], ProgramArgs["-secret"]);
            }
            else if (ProgramArgs.ContainsKey("-decrypt"))
            {
                if (!Verify.Decrypt(ProgramArgs["-input"], ProgramArgs["-output"], ProgramArgs["-secret"]))
                {
                    Environment.Exit(1);
                }
            }
            else if (ProgramArgs.ContainsKey("-dyndata"))
            {
                if (!Build.BuildDynamicHeaderFiles())
                {
                    Environment.Exit(1);
                }
            }
            else if (ProgramArgs.ContainsKey("-phapppub_gen"))
            {
                if (!Build.BuildPublicHeaderFiles())
                {
                    Environment.Exit(1);
                }
            }
            else if (ProgramArgs.ContainsKey("-sdk"))
            {
                if (ProgramArgs.ContainsKey("-x64"))
                {
                    if (ProgramArgs.ContainsKey("-release"))
                    {
                        Build.CopySidCapsFile(BuildFlags.Build64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyEtwTraceGuidsFile(BuildFlags.Build64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyIconFile(BuildFlags.Build64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.BuildSdk(BuildFlags.Build64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyKernelDriver(BuildFlags.Build64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyWow64Files(BuildFlags.Build64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                    }
                    else
                    {
                        Build.CopySidCapsFile(BuildFlags.Build64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyEtwTraceGuidsFile(BuildFlags.Build64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyIconFile(BuildFlags.Build64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.BuildSdk(BuildFlags.Build64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyKernelDriver(BuildFlags.Build64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyWow64Files(BuildFlags.Build64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                    }
                }
                else if (ProgramArgs.ContainsKey("-win32"))
                {
                    if (ProgramArgs.ContainsKey("-release"))
                    {
                        Build.CopySidCapsFile(BuildFlags.Build32bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyEtwTraceGuidsFile(BuildFlags.Build32bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyIconFile(BuildFlags.Build32bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.BuildSdk(BuildFlags.Build32bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyKernelDriver(BuildFlags.Build32bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                    }
                    else
                    {
                        Build.CopySidCapsFile(BuildFlags.Build32bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyEtwTraceGuidsFile(BuildFlags.Build32bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyIconFile(BuildFlags.Build32bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.BuildSdk(BuildFlags.Build32bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyKernelDriver(BuildFlags.Build32bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                    }
                }
                else if (ProgramArgs.ContainsKey("-arm64"))
                {
                    if (ProgramArgs.ContainsKey("-release"))
                    {
                        Build.CopySidCapsFile(BuildFlags.BuildArm64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyEtwTraceGuidsFile(BuildFlags.BuildArm64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyIconFile(BuildFlags.BuildArm64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.BuildSdk(BuildFlags.BuildArm64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyKernelDriver(BuildFlags.BuildArm64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                        Build.CopyWow64Files(BuildFlags.BuildArm64bit | BuildFlags.BuildRelease | BuildFlags.BuildVerbose);
                    }
                    else
                    {
                        Build.CopySidCapsFile(BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyEtwTraceGuidsFile(BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyIconFile(BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.BuildSdk(BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyKernelDriver(BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                        Build.CopyWow64Files(BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildVerbose);
                    }
                }
                else
                {
                    Build.CopySidCapsFile(BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildRelease);
                    Build.CopyEtwTraceGuidsFile(BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildRelease);
                    Build.CopyIconFile(BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildRelease);
                    Build.BuildSdk(BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildRelease);
                    Build.CopyKernelDriver(BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildRelease);
                    Build.CopyWow64Files(BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit | BuildFlags.BuildDebug | BuildFlags.BuildRelease);
                }
            }
            else if (ProgramArgs.ContainsKey("-sign_plugin"))
            {
                if (!Build.SignPlugin(ProgramArgs["-sign_plugin"]))
                {
                    Environment.Exit(1);
                }
            }
            else if (ProgramArgs.ContainsKey("-cleansdk"))
            {
                if (!Build.BuildSolution("SystemInformer.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildVerbose | BuildFlags.BuildApi
                    ))
                {
                    return;
                }

                Build.ShowBuildStats();
            }
            else if (ProgramArgs.ContainsKey("-bin"))
            {
                Build.SetupBuildEnvironment(false);

                if (!Build.BuildSolution("SystemInformer.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildVerbose | BuildFlags.BuildApi
                    ))
                    return;

                if (!Build.BuildSolution("plugins\\Plugins.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildVerbose | BuildFlags.BuildApi
                    ))
                    return;

                if (!Build.CopyTextFiles(true))
                    return;

                if (!Build.BuildBinZip())
                    return;

                Build.ShowBuildStats();
                Build.CopyTextFiles(false);
            }
            else if (ProgramArgs.ContainsKey("-debug"))
            {
                Build.SetupBuildEnvironment(true);

                if (!Build.BuildSolution("SystemInformer.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildDebug | BuildFlags.BuildVerbose |
                    BuildFlags.BuildApi
                    ))
                    return;

                if (!Build.BuildSolution("plugins\\Plugins.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildDebug | BuildFlags.BuildVerbose |
                    BuildFlags.BuildApi
                    ))
                {
                    return;
                }

                Build.ShowBuildStats();
            }
            else if (ProgramArgs.ContainsKey("-nightly-build"))
            {
                Build.SetupBuildEnvironment(true);

                if (!Build.BuildSolution("SystemInformer.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildVerbose | BuildFlags.BuildApi
                    ))
                    Environment.Exit(1);

                if (!Build.BuildSolution("plugins\\Plugins.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildVerbose | BuildFlags.BuildApi
                    ))
                    Environment.Exit(1);

                Build.CopyWow64Files( // required after plugin build (dmex)
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildDebug | BuildFlags.BuildRelease
                    );
            }
            else if (ProgramArgs.ContainsKey("-nightly-package"))
            {
                Build.SetupBuildEnvironment(true);

                if (!Build.ResignFiles())
                    Environment.Exit(1);
                if (!Build.CopyTextFiles(true))
                    Environment.Exit(1);
                if (!Build.BuildBinZip())
                    Environment.Exit(1);
                if (!Build.BuildSetupExe())
                    Environment.Exit(1);
            }
            else if (ProgramArgs.ContainsKey("-nightly-deploy"))
            {
                Build.SetupBuildEnvironment(true);

                if (!Build.BuildPdbZip(false))
                    Environment.Exit(1);
                //if (!Build.BuildSdkZip())
                //    Environment.Exit(1);
                //if (!Build.BuildSrcZip())
                //    Environment.Exit(1);
                //if (!Build.BuildChecksumsFile())
                //    Environment.Exit(1);
                //if (!Build.BuildDeployUploadArtifacts())
                //    Environment.Exit(1);
                if (!Build.BuildDeployUpdateConfig())
                    Environment.Exit(1);
            }
            else
            {
                Build.SetupBuildEnvironment(true);

                if (!Build.BuildSolution("SystemInformer.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildVerbose | BuildFlags.BuildApi
                    ))
                    return;

                if (!Build.BuildSolution("plugins\\Plugins.sln",
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildVerbose | BuildFlags.BuildApi
                    ))
                    return;

                Build.CopyWow64Files( // required after plugin build (dmex)
                    BuildFlags.Build32bit | BuildFlags.Build64bit | BuildFlags.BuildArm64bit |
                    BuildFlags.BuildDebug | BuildFlags.BuildRelease
                    );

                if (!Build.CopyTextFiles(true))
                    return;

                if (!Build.BuildBinZip())
                    return;
                if (!Build.BuildSetupExe())
                    return;
                Build.BuildPdbZip(false);
                //Build.BuildSdkZip();
                //Build.BuildSrcZip();
                Build.BuildChecksumsFile();
                Build.ShowBuildStats();
                Build.CopyTextFiles(false);
            }
        }

        private static Dictionary<string, string> ParseArgs(string[] args)
        {
            var dict = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            string argPending = null;

            foreach (string s in args)
            {
                if (s.StartsWith("-", StringComparison.OrdinalIgnoreCase))
                {
                    if (!dict.ContainsKey(s))
                        dict.Add(s, string.Empty);

                    argPending = s;
                }
                else
                {
                    if (argPending != null)
                    {
                        dict[argPending] = s;
                        argPending = null;
                    }
                    else
                    {
                        if (!dict.ContainsKey(string.Empty))
                            dict.Add(string.Empty, s);
                    }
                }
            }

            return dict;
        }

        public static void PrintColorMessage(string Message, ConsoleColor Color, bool Newline = true, BuildFlags Flags = BuildFlags.BuildVerbose)
        {
            if ((Flags & BuildFlags.BuildVerbose) != BuildFlags.BuildVerbose)
                return;

            Console.ForegroundColor = Color;
            if (Newline)
                Console.WriteLine(Message);
            else
                Console.Write(Message);
            Console.ResetColor();
        }
    }

    [Flags]
    public enum BuildFlags
    {
        None,
        Build32bit = 1,
        Build64bit = 2,
        BuildArm64bit = 4,
        BuildDebug = 8,
        BuildRelease = 16,
        BuildVerbose = 32,
        BuildApi = 64,
        BuildMsix = 128,
    }
}
