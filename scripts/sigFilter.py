import sys
import os
import re

class SigFilter():
  def __init__(self, name):
    self.srcfp = None
    self.reffp = None
    self.dstfp = None
    self.name = name
    self.numPerFile = 10000
    self.fileIdx = 0
    self.varNum = 0
    self.dstFileName = sys.argv[1] + "/model/" + name + "_checkSig"
    self.diffSigNum = 0

  def closeDstFile(self):
    if self.dstfp is not None:
      self.dstfp.writelines("return ret;\n}\n")
      self.dstfp.close()

  def newDstFile(self):
    self.closeDstFile()
    self.dstfp = open(self.dstFileName + str(self.fileIdx) + ".cpp", "w")
    self.dstfp.writelines("#include <iostream>\n#include <" + self.name + ".h>\n#include \"V" + self.name + "__Syms.h\"\n")
    self.dstfp.writelines("bool checkSig" + str(self.fileIdx) + "(bool display, V" + self.name + "* ref, S" + self.name + "* mod) {\n")
    self.dstfp.writelines("bool ret = false;\n")
    self.fileIdx += 1
    self.varNum = 0

  def width(self, data):
    endIdx = len(data) - 1
    while data[endIdx] != ':':
      endIdx -= 1
    startIdx = endIdx
    while data[startIdx-1] != '*':
      startIdx -= 1
    return int(data[startIdx : endIdx]) + 1

  def genDiffCode(self, modName, refName, line, mod_width, ref_width):
    self.diffSigNum += 1
    def gmp_wide_type(width):
      bits = int((width + 63) / 64) * 64
      return "GmpWideU<" + str(bits) + ">"

    wide_type = gmp_wide_type(max(mod_width, ref_width))
    ref_accum = line[3] + "_ref_gmp"
    if ref_width > 64:
      num = int((ref_width + 31) / 32)
      self.dstfp.writelines(wide_type + " " + ref_accum + " = 0;\n")
      for i in range(num - 1, -1, -1):
        self.dstfp.writelines(ref_accum + " = (" + ref_accum + " << 32) + " + refName + "[" + str(i) + "U];\n")
    else:
      self.dstfp.writelines(wide_type + " " + ref_accum + " = (" + wide_type + ")" + refName + ";\n")
    refName = ref_accum

    wide_bits = int((max(mod_width, ref_width) + 63) / 64) * 64
    if mod_width < wide_bits:
      mask = "((" + wide_type + ")1 << " + str(mod_width) + ") - 1"
    else:
      mask = "((" + wide_type + ")0 - 1)"

    self.dstfp.writelines("if( display || (((" + modName + " ^ " + refName + ") & " + mask + ") != 0)) {\n" +                           "  ret = true;\n" +                           "  std::cout << std::hex << \"" + line[2] + ": \" ")
    num = int((mod_width + 63) / 64)
    for i in range(num - 1, -1, -1):
      self.dstfp.writelines(" << (uint64_t)(" + modName + " >> " + str(i * 64) + ") << '_'")
    self.dstfp.writelines(" << \"  \" ")
    num = int((ref_width + 63) / 64)
    for i in range(num - 1, -1, -1):
      self.dstfp.writelines(" << (uint64_t)(" + refName + " >> " + str(i * 64) + ") << '_'")
    self.dstfp.writelines(" << std::endl;\n" + "} \n")

  def filter(self, srcFile, refFile):
    self.srcfp = open(srcFile, "r")
    self.reffp = open(refFile, "r")
    # self.dstfp = open(dstFile, "w")
    all_sigs = {}
    for line in self.reffp.readlines():
      match = re.search(r'/\*[0-9]*:[0-9]*\*/ ', line)
      if match:
        line = line.strip(" ;\n")
        line = line.split(" ")
        all_sigs[line[len(line) - 1]] = self.width(line[0])

    self.newDstFile()

    for line in self.srcfp.readlines():
      line = line.strip("\n")
      line = line.split(" ")
      sign = int(line[0])
      mod_width = int(line[1])
      if line[3] in all_sigs:
        if self.varNum == self.numPerFile:
          self.newDstFile()
        self.varNum += 1
        ref_width = all_sigs[line[3]]
        refName = "ref->rootp->" + line[3]
        modName = "mod->" + line[2]
        if mod_width > ref_width:
          continue
        assert(mod_width <= ref_width), "width(" + line[2] + ") = " + str(mod_width) + ", width(" + line[3] + ") = " + str(ref_width)

        self.genDiffCode(modName, refName, line, mod_width, ref_width)

    # self.dstfp.writelines("return ret;\n")
    self.srcfp.close()
    self.reffp.close()
    # self.dstfp.close()
    self.closeDstFile()
    self.dstfp = open(self.dstFileName + ".cpp", "w")
    self.dstfp.writelines("#include <iostream>\n#include <" + self.name + ".h>\n#include \"V" + self.name + "__Syms.h\"\n")
    for i in range (self.fileIdx):
      self.dstfp.writelines("bool checkSig" + str(i) + "(bool display, V" + self.name + "* ref, S" + self.name + "* mod);\n")
    self.dstfp.writelines("bool checkSig" + "(bool display, V" + self.name + "* ref, S" + self.name + "* mod){\nbool ret = false;\n")
    for i in range (self.fileIdx):
      self.dstfp.writelines("ret |= checkSig" + str(i) + "(display, ref, mod);\n")
    self.dstfp.writelines("return ret;\n}\n")
    self.dstfp.close()


if __name__ == "__main__":
  sigFilter = SigFilter(sys.argv[2])
  sigFilter.filter(sys.argv[1] + "/model/" + sys.argv[2] + "_sigs.txt",
                   sys.argv[1] + "/verilator/V" + sys.argv[2] + "___024root.h")
  print("diff sig num: " + str(sigFilter.diffSigNum))
